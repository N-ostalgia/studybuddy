#include "survey.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QVariant>
#include <QDebug>

class SurveyPrivate {
public:
    QSqlDatabase db;
};

Survey::Survey(QSqlDatabase& db, QObject *parent) : QObject(parent), db(db) {}

Survey::~Survey() {
    // No need to delete anything
}

QList<SurveyResult> Survey::getSurveyResultsForGoal(int goalId) const {
    QList<SurveyResult> results;
    QSqlQuery query(db);
    query.prepare("SELECT goal_id, timestamp, mood_emoji, distraction_level, distractions, session_satisfaction, goal_achieved, open_feedback, set_reminder FROM surveys WHERE goal_id = ? ORDER BY timestamp DESC");
    query.addBindValue(goalId);
    if (query.exec()) {
        while (query.next()) {
            SurveyResult r;
            r.goalId = query.value(0).toInt();
            r.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            r.moodEmoji = query.value(2).toString();
            r.distractionLevel = query.value(3).toInt();
            r.distractions = query.value(4).toString().split(", ", Qt::SkipEmptyParts);
            r.sessionSatisfaction = query.value(5).toInt();
            r.goalAchieved = query.value(6).toString();
            r.openFeedback = query.value(7).toString();
            r.setReminder = query.value(8).toInt() != 0;
            results.append(r);
        }
    }
    return results;
}

bool Survey::saveSurveyResult(const SurveyResult &result) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO surveys (goal_id, timestamp, mood_emoji, distraction_level, distractions, session_satisfaction, goal_achieved, open_feedback, set_reminder) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(result.goalId);
    query.addBindValue(result.timestamp.toString(Qt::ISODate));
    query.addBindValue(result.moodEmoji);
    query.addBindValue(result.distractionLevel);
    query.addBindValue(result.distractions.join(", "));
    query.addBindValue(result.sessionSatisfaction);
    query.addBindValue(result.goalAchieved);
    query.addBindValue(result.openFeedback);
    query.addBindValue(result.setReminder ? 1 : 0);
    if (!query.exec()) {
        qDebug() << "Survey: Failed to save survey result:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<SurveyResult> Survey::getAllSurveyResults() const {
    QList<SurveyResult> results;
    QSqlQuery query(db);
    query.prepare("SELECT goal_id, timestamp, mood_emoji, distraction_level, distractions, session_satisfaction, goal_achieved, open_feedback, set_reminder FROM surveys ORDER BY timestamp DESC");
    if (query.exec()) {
        while (query.next()) {
            SurveyResult r;
            r.goalId = query.value(0).toInt();
            r.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            r.moodEmoji = query.value(2).toString();
            r.distractionLevel = query.value(3).toInt();
            r.distractions = query.value(4).toString().split(", ", Qt::SkipEmptyParts);
            r.sessionSatisfaction = query.value(5).toInt();
            r.goalAchieved = query.value(6).toString();
            r.openFeedback = query.value(7).toString();
            r.setReminder = query.value(8).toInt() != 0;
            results.append(r);
        }
    }
    return results;
}

QList<QString> Survey::getCommonDistractions() const {
    return {"Phone", "Social Media", "Noise", "People", "Hunger", "Other"};
} 
bool Survey::deleteSurveyResult(int goalId, const QDateTime &timestamp) {
    QSqlQuery query(db);
    query.prepare("DELETE FROM surveys WHERE goal_id = ? AND timestamp = ?");
    query.addBindValue(goalId);
    query.addBindValue(timestamp.toString(Qt::ISODate));
    return query.exec();
}