#include "studysession.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSqlRecord>

StudySession::StudySession(QSqlDatabase& db, QObject* parent)
    : QObject(parent), db(db) {}

int StudySession::createSession(int plannedSessionId, const QString& type, const QString& notes)
{
    QSqlQuery query(db);
    QString startTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    query.prepare("INSERT INTO study_sessions (planned_session_id, start_time, type, notes) VALUES (?, ?, ?, ?)");
    query.addBindValue(plannedSessionId);
    query.addBindValue(startTime);
    query.addBindValue(type);
    query.addBindValue(notes);
    if (!query.exec()) {
        qDebug() << "Failed to create study session:" << query.lastError().text();
        return -1;
    }
    int id = query.lastInsertId().toInt();
    currentSessionId = id;
    return id;
}

bool StudySession::endSession(int sessionId)
{
    QSqlQuery query(db);
    QString endTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    query.prepare("UPDATE study_sessions SET end_time = ? WHERE id = ?");
    query.addBindValue(endTime);
    query.addBindValue(sessionId);
    if (!query.exec()) {
        qDebug() << "Failed to end study session:" << query.lastError().text();
        return false;
    }
    return true;
}

QVariantMap StudySession::getSession(int sessionId) const
{
    QVariantMap result;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM study_sessions WHERE id = ?");
    query.addBindValue(sessionId);
    if (query.exec() && query.next()) {
        for (int i = 0; i < query.record().count(); ++i)
            result[query.record().fieldName(i)] = query.value(i);
    }
    return result;
}

QList<QVariantMap> StudySession::getSessionsForDate(const QDate& date) const
{
    QList<QVariantMap> sessions;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM study_sessions WHERE date(start_time) = ?");
    query.addBindValue(date.toString("yyyy-MM-dd"));
    if (query.exec()) {
        while (query.next()) {
            QVariantMap result;
            for (int i = 0; i < query.record().count(); ++i)
                result[query.record().fieldName(i)] = query.value(i);
            sessions.append(result);
        }
    }
    return sessions;
}

int StudySession::getCurrentSessionId() const { return currentSessionId; }
void StudySession::setCurrentSessionId(int id) { currentSessionId = id; } 