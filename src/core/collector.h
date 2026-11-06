#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>

// Lance le collecteur PowerShell (collector/Get-BackupStatus.ps1) et renvoie son JSON.
// En mode démo, relit simplement un fichier JSON déjà collecté.
class Collector : public QObject
{
    Q_OBJECT
public:
    explicit Collector(QObject *parent = nullptr);

    void setDemoFile(const QString &path) { m_demoFile = path; }
    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }

    static QString scriptPath();
    static QString powershellProgram();

public slots:
    void collect(const QString &server);

signals:
    void started();
    void finished(const QJsonObject &status);
    void failed(const QString &message);

private:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void parseFile(const QString &path);

    QProcess m_process;
    QTemporaryDir m_tmp;
    QString m_outFile;
    QString m_demoFile;
};
