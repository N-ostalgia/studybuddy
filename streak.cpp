#include "streak.h"
#include <QDebug>
#include <QSqlError>
#include <QCoreApplication>

StudyStreak::StudyStreak(QSqlDatabase& db, QObject *parent)
    : QObject(parent), db(db), currentStreak(0), longestStreak(0)
{
    initializeStreaks();
}

StudyStreak::~StudyStreak()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool StudyStreak::initializeStreaks()
{
    QSqlQuery query(db);
    
    // Get the last study date and streak count
    query.exec("SELECT date, streak_count FROM study_streaks ORDER BY date DESC LIMIT 1");
    if (query.next()) {
        lastStudyDate = QDate::fromString(query.value("date").toString(), "yyyy-MM-dd");
        currentStreak = query.value("streak_count").toInt();
    }

    // Get the longest streak
    query.exec("SELECT MAX(streak_count) as longest FROM study_streaks");
    if (query.next()) {
        longestStreak = query.value("longest").toInt();
    }

    return true;
}

bool StudyStreak::recordStudySession(int minutes)
{
    QDate today = QDate::currentDate();
    QSqlQuery query(db);
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    query.prepare("SELECT study_minutes FROM study_streaks WHERE date = ?");
    query.addBindValue(today.toString("yyyy-MM-dd"));
    
    if (query.exec() && query.next()) {
        // Update existing record
        int currentMinutes = query.value(0).toInt();
        query.prepare("UPDATE study_streaks SET study_minutes = ? WHERE date = ?");
        query.addBindValue(currentMinutes + minutes);
        query.addBindValue(today.toString("yyyy-MM-dd"));
    } else {
        // Insert new record
        query.prepare("INSERT INTO study_streaks (date, study_minutes, streak_count, created_at) "
                     "VALUES (?, ?, ?, ?)");
        query.addBindValue(today.toString("yyyy-MM-dd"));
        query.addBindValue(minutes);
        query.addBindValue(currentStreak + 1);
        query.addBindValue(currentTime);
    }

    if (!query.exec()) {
        qDebug() << "Error recording study session:" << query.lastError().text();
        return false;
    }

    emit studyTimeUpdated(minutes);
    return checkAndUpdateStreak();
}

bool StudyStreak::checkAndUpdateStreak()
{
    QDate today = QDate::currentDate();
    
    if (lastStudyDate.addDays(1) == today) {
        currentStreak++;
        if (currentStreak > longestStreak) {
            longestStreak = currentStreak;
        }
        updateStreakCount();
        checkMilestones();
        emit streakUpdated(currentStreak);
        return true;
    }
    else if (lastStudyDate.addDays(1) < today) {
        if (currentStreak > 0) {
            emit streakBroken();
        }
        currentStreak = 1;
        updateStreakCount();
        emit streakUpdated(currentStreak);
        return true;
    }
    else if (lastStudyDate == today) {
        updateStreakCount();
        return true;
    }

    return false;
}

int StudyStreak::getCurrentStreak() const
{
    return currentStreak;
}

int StudyStreak::getLongestStreak() const
{
    return longestStreak;
}

int StudyStreak::getTotalStudyDays() const
{
    QSqlQuery query(db);
    query.exec("SELECT COUNT(DISTINCT date) as total FROM study_streaks");
    
    if (query.next()) {
        return query.value("total").toInt();
    }
    return 0;
}

QDate StudyStreak::getLastStudyDate() const
{
    return lastStudyDate;
}

QMap<QDate, int> StudyStreak::getStudyHistory(const QDate &startDate, const QDate &endDate)
{
    QMap<QDate, int> history;
    QSqlQuery query(db);
    
    query.prepare("SELECT date, study_minutes FROM study_streaks "
                 "WHERE date BETWEEN ? AND ? ORDER BY date");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    
    if (query.exec()) {
        while (query.next()) {
            QDate date = QDate::fromString(query.value("date").toString(), "yyyy-MM-dd");
            int minutes = query.value("study_minutes").toInt();
            history[date] = minutes;
        }
    }
    
    return history;
}

int StudyStreak::getAverageStudyTime() const
{
    QSqlQuery query(db);
    query.exec("SELECT AVG(study_minutes) as average FROM study_streaks");
    
    if (query.next()) {
        return query.value("average").toInt();
    }
    return 0;
}

int StudyStreak::getTotalStudyTime() const
{
    QSqlQuery query(db);
    query.exec("SELECT SUM(study_minutes) as total FROM study_streaks");
    
    if (query.next()) {
        return query.value("total").toInt();
    }
    return 0;
}

bool StudyStreak::hasReachedMilestone(int days) const
{
    return currentStreak >= days;
}

QList<int> StudyStreak::getReachedMilestones() const
{
    QList<int> milestones = getMilestones();
    QList<int> reached;
    
    for (int milestone : milestones) {
        if (currentStreak >= milestone) {
            reached.append(milestone);
        }
    }
    
    return reached;
}

QList<int> StudyStreak::getUpcomingMilestones() const
{
    QList<int> milestones = getMilestones();
    QList<int> upcoming;
    
    for (int milestone : milestones) {
        if (currentStreak < milestone) {
            upcoming.append(milestone);
        }
    }
    
    return upcoming;
}

bool StudyStreak::updateStreakCount()
{
    QSqlQuery query(db);
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    
    query.prepare("UPDATE study_streaks SET streak_count = ? WHERE date = ?");
    query.addBindValue(currentStreak);
    query.addBindValue(today);
    
    return query.exec();
}

bool StudyStreak::checkMilestones()
{
    QList<int> milestones = getMilestones();
    QList<int> newMilestones;
    
    for (int milestone : milestones) {
        if (currentStreak == milestone) {
            newMilestones.append(milestone);
        }
    }
    
    if (!newMilestones.isEmpty()) {
        emitMilestoneSignals(newMilestones);
    }
    
    return true;
}

void StudyStreak::emitMilestoneSignals(const QList<int> &newMilestones)
{
    for (int milestone : newMilestones) {
        emit milestoneReached(milestone);
    }
}

QList<int> StudyStreak::getMilestones() const
{
    // Define milestone days
    return {1, 3, 7, 14, 30, 100, 365};
} 
