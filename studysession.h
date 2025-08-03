#ifndef STUDYSESSION_H
#define STUDYSESSION_H

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

class StudySession : public QObject {
    Q_OBJECT
public:
    explicit StudySession(QSqlDatabase& db, QObject* parent = nullptr);
    int createSession(int plannedSessionId, const QString& type, const QString& notes = "");
    bool endSession(int sessionId);
    QVariantMap getSession(int sessionId) const;
    QList<QVariantMap> getSessionsForDate(const QDate& date) const;
    int getCurrentSessionId() const;
    void setCurrentSessionId(int id);
private:
    QSqlDatabase& db;
    int currentSessionId = -1;
};

#endif // STUDYSESSION_H 