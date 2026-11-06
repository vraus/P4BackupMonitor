#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

// Règles d'interprétation de l'état collecté : ce qui est vert, orange ou rouge.
namespace Health {

enum class Level { Ok = 0, Warning = 1, Error = 2, Unknown = 3 };

struct Finding
{
    Level level = Level::Ok;
    QString area;    // "Sauvegarde", "Tâches", "Serveur", "NAS", "Journaux", "Collecte"
    QString message; // phrase lisible
    QString code;    // code du guide de dépannage (SAV-01...), facultatif
};

struct Summary
{
    Level overall = Level::Unknown;
    QList<Finding> findings; // problèmes uniquement, du plus grave au moins grave
};

Summary evaluate(const QJsonObject &status);

Level backupAgeLevel(double ageHours, double maxAgeHours);
Level freeSpaceLevel(double freeBytes, double totalBytes);
Level logLevel(const QString &status);

QString taskResultText(qint64 result);
Level taskResultLevel(qint64 result);

QString formatBytes(double bytes);
QString formatAge(double hours);
QDateTime parseDate(const QString &iso);

} // namespace Health
