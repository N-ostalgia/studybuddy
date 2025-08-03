#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QString>
#include <QMap>

class Achievement : public QObject
{
    Q_OBJECT

public:
    explicit Achievement(QSqlDatabase& db, QObject *parent = nullptr);
    ~Achievement();

    // Achievement Management
    bool initializeAchievements();
    bool unlockAchievement(const QString &achievementId);
    bool isAchievementUnlocked(const QString &achievementId) const;
    
    // Achievement Categories
    enum class Category {
        Focus,
        Streak,
        Time,
        Special
    };
    
    // Achievement Types
    struct AchievementInfo {
        QString id;
        QString name;
        QString description;
        Category category;
        int requiredValue;
        QString iconPath;
    };

    // Achievement Tracking
    void trackFocusScore(float score);
    void trackStudyTime(int minutes);
    void trackStreak(int days);
    void checkAchievements();
    
    // Achievement Retrieval
    QList<AchievementInfo> getUnlockedAchievements() const;
    QList<AchievementInfo> getLockedAchievements() const;
    QList<AchievementInfo> getAchievementsByCategory(Category category) const;
    
    // Statistics
    int getTotalAchievements() const;
    int getUnlockedCount() const;
    float getCompletionPercentage() const;
    int getAchievementProgress(const QString &achievementId) const;

signals:
    void achievementUnlocked(const QString &achievementId, const QString &name);
    void progressUpdated(const QString &achievementId, int progress);
    void allAchievementsUnlocked();
    void sessionCompleted();
    void pomodoroCompleted();

private:
    QSqlDatabase& db;
    QMap<QString, AchievementInfo> achievementDefinitions;
    QMap<QString, int> achievementProgress;
    
    void loadAchievementDefinitions();
    bool checkAchievementConditions(const QString &achievementId);
    void updateProgress(const QString &achievementId, int progress);
    QString categoryToString(Category category) const;
    Category stringToCategory(const QString &category) const;
    void trackFirstSession();
    void trackFirstPomodoro();
};

Q_DECLARE_METATYPE(Achievement::AchievementInfo)

#endif // ACHIEVEMENTS_H
