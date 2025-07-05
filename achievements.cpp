#include "achievements.h"
#include <QDebug>
#include <QSqlError>

Achievement::Achievement(QObject *parent)
    : QObject(parent)
{
    initializeDatabase();
    loadAchievementDefinitions();

    connect(this, &Achievement::sessionCompleted, this, [this]() {
        trackFirstSession();
    });

    connect(this, &Achievement::pomodoroCompleted, this, [this]() {
        trackFirstPomodoro();
    });
}

Achievement::~Achievement()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool Achievement::initializeDatabase()
{
    if (QSqlDatabase::contains("achievements_connection")) {
        db = QSqlDatabase::database("achievements_connection");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", "achievements_connection");
        db.setDatabaseName("achievements.db");
    }

    if (!db.open()) {
        qDebug() << "Error: Failed to open database:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    QString createTableQuery = 
        "CREATE TABLE IF NOT EXISTS achievements ("
        "id TEXT PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "description TEXT NOT NULL, "
        "category TEXT NOT NULL, "
        "required_value INTEGER NOT NULL, "
        "icon_path TEXT, "
        "unlocked_at TEXT, "
        "progress INTEGER DEFAULT 0"
        ")";

    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Failed to create table:" << query.lastError().text();
        return false;
    }

    return true;
}

void Achievement::loadAchievementDefinitions()
{
    // default achievements
    QList<AchievementInfo> defaultAchievements = {
        {"focus_master", "Focus Master", "Maintain 95%+ focus for 30 minutes", Category::Focus, 30, ":/icons/focus_master.png"},
        {"streak_7", "Week Warrior", "Study for 7 consecutive days", Category::Streak, 7, ":/icons/streak_7.png"},
        {"time_4h", "Marathon Learner", "Complete a 4-hour study session", Category::Time, 240, ":/icons/time_4h.png"},
        {"night_owl", "Night Owl", "Study after 10 PM for 2 hours", Category::Special, 120, ":/icons/night_owl.png"},
        {"early_bird", "Early Bird", "Study before 8 AM for 2 hours", Category::Special, 120, ":/icons/early_bird.png"},
        {"perfect_day", "Perfect Day", "Complete all daily goals", Category::Special, 1, ":/icons/perfect_day.png"},
        {"first_session", "First Session", "Complete your first camera session", Category::Special, 1, ":/icons/first_session.png"},
        {"first_pomodoro", "First Pomodoro", "Complete your first Pomodoro session", Category::Special, 1, ":/icons/first_pomodoro.png"}
    };
    QSqlQuery query(db);
    for (const auto &achievement : defaultAchievements) {
        query.prepare("INSERT OR IGNORE INTO achievements (id, name, description, category, required_value, icon_path) "
                     "VALUES (?, ?, ?, ?, ?, ?)");
        query.addBindValue(achievement.id);
        query.addBindValue(achievement.name);
        query.addBindValue(achievement.description);
        query.addBindValue(categoryToString(achievement.category));
        query.addBindValue(achievement.requiredValue);
        query.addBindValue(achievement.iconPath);
        
        if (!query.exec()) {
            qDebug() << "Error inserting achievement:" << query.lastError().text();
        }
    }

    query.exec("SELECT * FROM achievements");
    while (query.next()) {
        AchievementInfo info;
        info.id = query.value("id").toString();
        info.name = query.value("name").toString();
        info.description = query.value("description").toString();
        info.category = stringToCategory(query.value("category").toString());
        info.requiredValue = query.value("required_value").toInt();
        info.iconPath = query.value("icon_path").toString();
        
        achievementDefinitions[info.id] = info;
        achievementProgress[info.id] = query.value("progress").toInt();
    }
}

bool Achievement::unlockAchievement(const QString &achievementId)
{
    if (!achievementDefinitions.contains(achievementId)) {
        return false;
    }

    QSqlQuery query(db);
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    query.prepare("UPDATE achievements SET unlocked_at = ? WHERE id = ? AND unlocked_at IS NULL");
    query.addBindValue(currentTime);
    query.addBindValue(achievementId);

    if (!query.exec()) {
        qDebug() << "Error unlocking achievement:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() > 0) {
        emit achievementUnlocked(achievementId, achievementDefinitions[achievementId].name);
        
        if (getUnlockedCount() == getTotalAchievements()) {
            emit allAchievementsUnlocked();
        }
        return true;
    }

    return false;
}

bool Achievement::isAchievementUnlocked(const QString &achievementId) const
{
    QSqlQuery query(db);
    query.prepare("SELECT unlocked_at FROM achievements WHERE id = ?");
    query.addBindValue(achievementId);

    if (query.exec() && query.next()) {
        return !query.value(0).isNull();
    }
    return false;
}

void Achievement::trackFocusScore(float score)
{
    if (score >= 0.95f) {
        updateProgress("focus_master", 1);
        checkAchievements();
    }
}

void Achievement::trackStudyTime(int minutes)
{
    if (minutes >= 240) {
        updateProgress("time_4h", minutes);
        checkAchievements();
    }
}

void Achievement::trackStreak(int days)
{
    if (days >= 7) {
        updateProgress("streak_7", days);
        checkAchievements();
    }
}

void Achievement::checkAchievements()
{
    QTime currentTime = QTime::currentTime();
    
    // Check time-based achievements
    if (currentTime.hour() >= 22) {
        updateProgress("night_owl", 1);
    }
    if (currentTime.hour() < 8) {
        updateProgress("early_bird", 1);
    }

    // Check all achievements for unlocking
    for (const auto &achievement : achievementDefinitions) {
        if (!isAchievementUnlocked(achievement.id) && 
            checkAchievementConditions(achievement.id)) {
            unlockAchievement(achievement.id);
        }
    }
}

QList<Achievement::AchievementInfo> Achievement::getUnlockedAchievements() const
{
    QList<AchievementInfo> unlocked;
    QSqlQuery query(db);
    query.prepare("SELECT id FROM achievements WHERE unlocked_at IS NOT NULL");

    if (query.exec()) {
        while (query.next()) {
            QString id = query.value(0).toString();
            if (achievementDefinitions.contains(id)) {
                unlocked.append(achievementDefinitions[id]);
            }
        }
    }

    return unlocked;
}

QList<Achievement::AchievementInfo> Achievement::getLockedAchievements() const
{
    QList<AchievementInfo> locked;
    QSqlQuery query(db);
    query.prepare("SELECT id FROM achievements WHERE unlocked_at IS NULL");

    if (query.exec()) {
        while (query.next()) {
            QString id = query.value(0).toString();
            if (achievementDefinitions.contains(id)) {
                locked.append(achievementDefinitions[id]);
            }
        }
    }

    return locked;
}

QList<Achievement::AchievementInfo> Achievement::getAchievementsByCategory(Category category) const
{
    QList<AchievementInfo> achievements;
    for (const auto &achievement : achievementDefinitions) {
        if (achievement.category == category) {
            achievements.append(achievement);
        }
    }
    return achievements;
}

int Achievement::getTotalAchievements() const
{
    return achievementDefinitions.size();
}

int Achievement::getUnlockedCount() const
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM achievements WHERE unlocked_at IS NOT NULL");
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

float Achievement::getCompletionPercentage() const
{
    int total = getTotalAchievements();
    if (total == 0) return 0.0f;
    
    return (float)getUnlockedCount() / total * 100.0f;
}

int Achievement::getAchievementProgress(const QString &achievementId) const
{
    return achievementProgress.value(achievementId, 0);
}

bool Achievement::checkAchievementConditions(const QString &achievementId)
{
    if (!achievementDefinitions.contains(achievementId)) {
        return false;
    }

    const AchievementInfo &achievement = achievementDefinitions[achievementId];
    int progress = achievementProgress[achievementId];

    return progress >= achievement.requiredValue;
}

void Achievement::updateProgress(const QString &achievementId, int progress)
{
    if (!achievementDefinitions.contains(achievementId)) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE achievements SET progress = ? WHERE id = ?");
    query.addBindValue(progress);
    query.addBindValue(achievementId);

    if (query.exec()) {
        achievementProgress[achievementId] = progress;
        emit progressUpdated(achievementId, progress);
    }
}

QString Achievement::categoryToString(Category category) const
{
    switch (category) {
        case Category::Focus: return "Focus";
        case Category::Streak: return "Streak";
        case Category::Time: return "Time";
        case Category::Special: return "Special";
        default: return "Unknown";
    }
}

Achievement::Category Achievement::stringToCategory(const QString &category) const
{
    if (category == "Focus") return Category::Focus;
    if (category == "Streak") return Category::Streak;
    if (category == "Time") return Category::Time;
    if (category == "Special") return Category::Special;
    return Category::Special; // Default category
}

void Achievement::trackFirstSession()
{
    if (!isAchievementUnlocked("first_session")) {
        updateProgress("first_session", 1);
        checkAchievements();
    }
}

void Achievement::trackFirstPomodoro()
{
    if (!isAchievementUnlocked("first_pomodoro")) {
        updateProgress("first_pomodoro", 1);
        checkAchievements();
    }
}

bool Achievement::initializeAchievements()
{
    if (!initializeDatabase()) {
        qDebug() << "Error: Failed to initialize database for achievements.";
        return false;
    }

    loadAchievementDefinitions();
    return true;
} 
