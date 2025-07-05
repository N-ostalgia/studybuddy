#ifndef STREAK_H
#define STREAK_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QString>
#include <QMap>

class StudyStreak : public QObject
{
    Q_OBJECT

public:
    explicit StudyStreak(QObject *parent = nullptr);
    ~StudyStreak();

    // Streak Management
    bool initializeStreaks();
    bool recordStudySession(int minutes);
    bool checkAndUpdateStreak();
    
    // Streak Information
    int getCurrentStreak() const;
    int getLongestStreak() const;
    int getTotalStudyDays() const;
    QDate getLastStudyDate() const;
    
    // Statistics
    QMap<QDate, int> getStudyHistory(const QDate &startDate, const QDate &endDate);
    int getAverageStudyTime() const;
    int getTotalStudyTime() const;
    
    // Milestones
    bool hasReachedMilestone(int days) const;
    QList<int> getReachedMilestones() const;
    QList<int> getUpcomingMilestones() const;

signals:
    void streakUpdated(int currentStreak);
    void milestoneReached(int days);
    void streakBroken();
    void studyTimeUpdated(int totalMinutes);

private:
    QSqlDatabase db;
    int currentStreak;
    int longestStreak;
    QDate lastStudyDate;
    
    bool initializeDatabase();
    bool updateStreakCount();
    bool checkMilestones();
    void emitMilestoneSignals(const QList<int> &newMilestones);
    QList<int> getMilestones() const;
};

#endif // STREAK_H 