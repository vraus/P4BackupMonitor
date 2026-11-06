#include "collector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>

namespace {
constexpr int kTimeoutMs = 120000; // un serveur éteint fait attendre le délai CIM, pas plus
}

Collector::Collector(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::finished, this, &Collector::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            emit failed(tr("PowerShell introuvable (%1).").arg(powershellProgram()));
    });
}

QString Collector::scriptPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("collector/Get-BackupStatus.ps1");
}

QString Collector::powershellProgram()
{
#ifdef Q_OS_WIN
    // Windows PowerShell 5.1 : présent sur tous les postes, module ScheduledTasks inclus
    return QStringLiteral("powershell.exe");
#else
    return QStringLiteral("pwsh"); // développement hors Windows
#endif
}

void Collector::collect(const QString &server)
{
    if (isRunning())
        return;
    emit started();

    if (!m_demoFile.isEmpty()) {
        QTimer::singleShot(0, this, [this, file = m_demoFile] { parseFile(file); });
        return;
    }
    if (server.trimmed().isEmpty()) {
        emit failed(tr("Indiquer le nom du serveur Perforce."));
        return;
    }
    if (!QFileInfo::exists(scriptPath())) {
        emit failed(tr("Collecteur introuvable : %1").arg(QDir::toNativeSeparators(scriptPath())));
        return;
    }

    m_outFile = m_tmp.filePath("status.json");
    QFile::remove(m_outFile);
    const QStringList args{
        "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
        "-File", QDir::toNativeSeparators(scriptPath()),
        "-Server", server.trimmed(),
        "-OutFile", QDir::toNativeSeparators(m_outFile),
    };
    m_process.start(powershellProgram(), args);
    QTimer::singleShot(kTimeoutMs, &m_process, [this] {
        if (isRunning()) {
            m_process.kill();
            emit failed(tr("Le collecteur ne répond plus après %1 s.").arg(kTimeoutMs / 1000));
        }
    });
}

void Collector::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status != QProcess::NormalExit)
        return; // déjà signalé (délai dépassé ou arrêt)
    if (!QFileInfo::exists(m_outFile)) {
        const QString err = QString::fromLocal8Bit(m_process.readAllStandardError()).trimmed();
        emit failed(tr("Le collecteur a échoué (code %1).\n%2").arg(exitCode).arg(err));
        return;
    }
    parseFile(m_outFile);
}

void Collector::parseFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit failed(tr("Résultat illisible : %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    QByteArray data = f.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit failed(tr("Résultat du collecteur invalide : %1").arg(err.errorString()));
        return;
    }
    emit finished(doc.object());
}