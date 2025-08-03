#ifndef STUDYGOALS_H
#define STUDYGOALS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QSqlRecord>
#include <QDate>
#include <QTimer>
#include "pomodoro.h"


struct GoalInfo {
    int id;
    QString subject;
    int targetMinutes;
    int completedMinutes;
    QString startDate;
    QString dueDate;
    QString status;
    QString notes;
    QString recurrenceType;
    QString recurrenceValue;
    QString category;
    QDate lastGeneratedDate;
    QStringList resources; // New field for resources
};

Q_DECLARE_METATYPE(GoalInfo)

class StudyGoal : public QObject
{
    Q_OBJECT

public:
    explicit StudyGoal(QSqlDatabase& db, QObject *parent = nullptr);
    ~StudyGoal();

    // CRUD Operations
    bool createGoal(const QString &subject, int targetMinutes, const QString &notes = "", const QString &recurrenceType = "None", const QString &recurrenceValue = "", const QString &category = "Uncategorized", const QStringList &resources = QStringList());
    bool updateGoal(int goalId, const QString &subject, int targetMinutes, const QString &notes, const QString &recurrenceType, const QString &recurrenceValue, const QString &category, const QStringList &resources = QStringList());
    bool deleteGoal(int goalId);
    
    // Goal Retrieval
    QList<GoalInfo> getGoalsForDate(const QDate &date) const;
    QList<QMap<QString, QVariant>> getGoalsForSubject(const QString &subject) const;
    QMap<QString, QVariant> getGoalDetails(int goalId) const;
    QList<QString> getResourcesForGoal(int goalId) const;
    
    // Progress Tracking
    int getProgress(int goalId) const;
    int getTarget(int goalId) const;
    float getCompletionPercentage(int goalId) const;
    bool addProgress(int goalId, int minutesToAdd);

    // tracking methods for Pomodoro integration
    void connectToPomodoroTimer(PomodoroTimer *timer);
    void startTrackingGoal(int goalId, int initialElapsedSeconds = 0);
    // Now adds progress when tracking is stopped
    void stopTrackingGoal();

    // Recurring Goals
    void generateRecurringGoals(int parentGoalId);
    void checkAndGenerateAllRecurringGoals(); 

    // Statistics
    QMap<QString, int> getSubjectStats(const QDate &startDate, const QDate &endDate);
    QMap<QString, int> getDailyStats(const QDate &date);

signals:
    void goalCreated(int goalId);
    void goalUpdated(int goalId);
    void goalDeleted(int goalId);
    
    // signals for session tracking
    void sessionTimeUpdated(int goalId, int elapsedSeconds);
    void sessionProgressUpdated(int goalId, int sessionMinutes);
    void goalTrackingStarted(int goalId);
    void goalTrackingStopped();

private:
    QSqlDatabase& db;
    bool initializeDatabase();
    bool executeQuery(const QString &query, QSqlQuery &result);

    int m_currentTrackingGoalId;
    PomodoroTimer *m_pomodoroTimer;
    QTime m_pomodoroSessionStartTime;

private slots:
    void handlePomodoroTimerCompletedInternally();
    void handlePomodoroTimeUpdated(const QString &time);
};

#endif // STUDYGOALS_H
