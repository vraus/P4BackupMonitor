#include "health.h"

#include <QJsonArray>
#include <QLocale>
#include <QMap>
#include <algorithm>

namespace Health {

namespace {
constexpr double kWarnFreeRatio = 0.20;
constexpr double kErrorFreeRatio = 0.10;
constexpr double kDailyTaskMaxHours = 26.0;
constexpr double kWeeklyTaskMaxHours = 8 * 24 + 2;
constexpr double kIncompleteLogHours = 6.0; // un journal sans bilan plus vieux que ça = exécution interrompue

void add(Summary &s, Level l, const QString &area, const QString &msg, const QString &code = {})
{
    s.findings.append({l, area, msg, code});
}
} // namespace

QDateTime parseDate(const QString &iso)
{
    if (iso.isEmpty())
        return {};
    QDateTime d = QDateTime::fromString(iso, Qt::ISODate);
    d.setTimeSpec(Qt::LocalTime);
    return d;
}

QString formatBytes(double bytes)
{
    return QLocale().formattedDataSize(qint64(bytes), 1, QLocale::DataSizeTraditionalFormat);
}

QString formatAge(double hours)
{
    if (hours < 1.0)
        return QObject::tr("%1 min").arg(int(hours * 60));
    if (hours < 48.0)
        return QObject::tr("%1 h").arg(QLocale().toString(hours, 'f', 1));
    return QObject::tr("%1 jours").arg(QLocale().toString(hours / 24.0, 'f', 1));
}

Level backupAgeLevel(double ageHours, double maxAgeHours)
{
    if (maxAgeHours <= 0)
        maxAgeHours = kDailyTaskMaxHours;
    return ageHours <= maxAgeHours ? Level::Ok : Level::Error;
}

Level freeSpaceLevel(double freeBytes, double totalBytes)
{
    if (totalBytes <= 0)
        return Level::Unknown;
    const double r = freeBytes / totalBytes;
    if (r < kErrorFreeRatio)
        return Level::Error;
    if (r < kWarnFreeRatio)
        return Level::Warning;
    return Level::Ok;
}

Level logLevel(const QString &status)
{
    if (status == "ok")
        return Level::Ok;
    if (status == "warning")
        return Level::Warning;
    if (status == "error")
        return Level::Error;
    return Level::Unknown; // "incomplete" : en cours ou interrompu
}

QString taskResultText(qint64 r)
{
    const quint32 u = quint32(r);
    switch (u) {
    case 0x0: return QObject::tr("Réussie");
    case 0x1: return QObject::tr("Échec du script (voir le journal)");
    case 0x41300: return QObject::tr("Prête");
    case 0x41301: return QObject::tr("En cours");
    case 0x41303: return QObject::tr("Jamais exécutée");
    case 0x41306: return QObject::tr("Arrêtée");
    case 0x8004131F: return QObject::tr("Déjà en cours, ignorée");
    case 0x80070569: return QObject::tr("Droit d'exécution en tâche refusé");
    case 0x8007052E: return QObject::tr("Ouverture de session refusée (mot de passe)");
    case 0x800710E0: return QObject::tr("Refusée par le Planificateur");
    default: return QObject::tr("Code 0x%1").arg(u, 0, 16);
    }
}

Level taskResultLevel(qint64 r)
{
    const quint32 u = quint32(r);
    if (u == 0x0 || u == 0x41301)
        return Level::Ok;
    if (u == 0x41303 || u == 0x41300 || u == 0x8004131F)
        return Level::Warning;
    return Level::Error;
}

static QString taskResultCode(qint64 r)
{
    switch (quint32(r)) {
    case 0x80070569: return "MES-07";
    case 0x8007052E: return "TEST-02";
    default: return {};
    }
}

Summary evaluate(const QJsonObject &st)
{
    Summary s;
    const QDateTime now = parseDate(st.value("collectedAt").toString()).isValid()
                              ? parseDate(st.value("collectedAt").toString())
                              : QDateTime::currentDateTime();

    // Problèmes de collecte : une source inaccessible est en soi une alerte
    for (const auto &v : st.value("problems").toArray()) {
        const QJsonObject p = v.toObject();
        const QString src = p.value("source").toString();
        const QString code = src == "nas" ? "SAV-01" : src == "status" ? "TEST-01" : QString();
        add(s, Level::Error, QObject::tr("Collecte"), p.value("message").toString(), code);
    }

    // Dernière sauvegarde
    const QJsonObject cfg = st.value("config").toObject();
    const double maxAge = cfg.value("maxAgeHours").toDouble(kDailyTaskMaxHours);
    const QJsonObject lb = st.value("lastBackup").toObject();
    if (lb.isEmpty()) {
        if (st.value("nas").toObject().value("reachable").toBool())
            add(s, Level::Error, QObject::tr("Sauvegarde"), QObject::tr("Aucune sauvegarde réussie trouvée sur le NAS."), "TEST-01");
    } else {
        const double age = lb.value("ageHours").toDouble();
        if (backupAgeLevel(age, maxAge) != Level::Ok)
            add(s, Level::Error, QObject::tr("Sauvegarde"),
                QObject::tr("Dernière sauvegarde réussie il y a %1 (seuil %2 h).").arg(formatAge(age)).arg(maxAge),
                "TEST-02");
    }

    // Tâches planifiées
    const QJsonArray tasks = st.value("tasks").toArray();
    for (const auto &v : tasks) {
        const QJsonObject t = v.toObject();
        const QString name = t.value("name").toString();
        const qint64 res = qint64(t.value("lastResult").toDouble());
        if (t.value("state").toString() == "Disabled")
            add(s, Level::Warning, QObject::tr("Tâches"), QObject::tr("« %1 » est désactivée.").arg(name));
        const Level rl = taskResultLevel(res);
        if (rl != Level::Ok)
            add(s, rl, QObject::tr("Tâches"),
                QObject::tr("« %1 » : %2.").arg(name, taskResultText(res)), taskResultCode(res));
        const QDateTime last = parseDate(t.value("lastRun").toString());
        const double maxH = name.contains("complet", Qt::CaseInsensitive) ? kWeeklyTaskMaxHours : kDailyTaskMaxHours;
        if (last.isValid() && last.secsTo(now) / 3600.0 > maxH)
            add(s, Level::Error, QObject::tr("Tâches"),
                QObject::tr("« %1 » n'a pas tourné depuis %2.").arg(name, formatAge(last.secsTo(now) / 3600.0)),
                "GEN-03");
    }

    // Disques du serveur
    for (const auto &v : st.value("disks").toArray()) {
        const QJsonObject d = v.toObject();
        const Level l = freeSpaceLevel(d.value("freeBytes").toDouble(), d.value("totalBytes").toDouble());
        if (l == Level::Warning || l == Level::Error)
            add(s, l, QObject::tr("Serveur"),
                QObject::tr("Disque %1 : %2 libres sur %3.")
                    .arg(d.value("name").toString(), formatBytes(d.value("freeBytes").toDouble()),
                         formatBytes(d.value("totalBytes").toDouble())),
                "B7");
    }

    // NAS
    const QJsonObject nas = st.value("nas").toObject();
    if (nas.value("reachable").toBool()) {
        const Level l = freeSpaceLevel(nas.value("freeBytes").toDouble(), nas.value("totalBytes").toDouble());
        if (l == Level::Warning || l == Level::Error)
            add(s, l, QObject::tr("NAS"),
                QObject::tr("%1 libres sur %2.").arg(formatBytes(nas.value("freeBytes").toDouble()),
                                                    formatBytes(nas.value("totalBytes").toDouble())),
                "NAS-03");
    }

    // Journaux : seul le plus récent de chaque type compte
    QMap<QString, QJsonObject> latest;
    for (const auto &v : st.value("logs").toArray()) {
        const QJsonObject l = v.toObject();
        const QString kind = l.value("kind").toString();
        if (!latest.contains(kind))
            latest.insert(kind, l); // le collecteur trie du plus récent au plus ancien
    }
    for (auto it = latest.cbegin(); it != latest.cend(); ++it) {
        const QJsonObject l = it.value();
        const QString status = l.value("status").toString();
        QString code;
        for (const auto &iv : l.value("issues").toArray()) {
            code = iv.toObject().value("code").toString();
            if (!code.isEmpty())
                break;
        }
        if (status == "error")
            add(s, Level::Error, QObject::tr("Journaux"), QObject::tr("Dernier journal « %1 » en échec.").arg(it.key()), code);
        else if (status == "warning")
            add(s, Level::Warning, QObject::tr("Journaux"), QObject::tr("Dernier journal « %1 » : avertissement.").arg(it.key()), code);
        else if (status == "incomplete") {
            const QDateTime mod = parseDate(l.value("modified").toString());
            if (mod.isValid() && mod.secsTo(now) / 3600.0 > kIncompleteLogHours)
                add(s, Level::Warning, QObject::tr("Journaux"),
                    QObject::tr("Dernier journal « %1 » sans bilan : exécution interrompue.").arg(it.key()));
        }
    }

    std::stable_sort(s.findings.begin(), s.findings.end(),
                     [](const Finding &a, const Finding &b) { return int(a.level) > int(b.level); });
    s.overall = Level::Ok;
    for (const Finding &f : s.findings)
        s.overall = Level(std::max(int(s.overall), int(f.level)));
    if (st.isEmpty())
        s.overall = Level::Unknown;
    return s;
}

} // namespace Health
