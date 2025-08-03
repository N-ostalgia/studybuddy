#include "studygoals.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>
#include <QCoreApplication>

StudyGoal::StudyGoal(QSqlDatabase& db, QObject *parent)
    : QObject(parent), db(db), m_currentTrackingGoalId(-1), m_pomodoroTimer(nullptr)
{
}

StudyGoal::~StudyGoal()
{
    if (db.isOpen()) {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase("StudyGoalsConnection");
    }
}

bool StudyGoal::executeQuery(const QString &queryStr, QSqlQuery &result)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for executing query.";
        return false;
    }
    result.prepare(queryStr);
    if (!result.exec()) {
        qDebug() << "Query failed:" << result.lastError().text() << "Query:" << queryStr;
        return false;
    }
    return true;
}

// Goal Management
bool StudyGoal::createGoal(const QString &subject, int targetMinutes, const QString &notes, const QString &recurrenceType, const QString &recurrenceValue, const QString &category, const QStringList &resources)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for creating goal.";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO goals (subject, target_minutes, notes, start_date, recurrence_type, recurrence_value, last_generated_date, category) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(subject);
    query.addBindValue(targetMinutes);
    query.addBindValue(notes);
    query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    query.addBindValue(recurrenceType);
    query.addBindValue(recurrenceValue);
    query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    query.addBindValue(category);

    if (!query.exec()) {
        qDebug() << "Error creating goal:" << query.lastError().text();
        return false;
    }
    int newGoalId = query.lastInsertId().toInt();
    // Store resources
    for (const QString &res : resources) {
        QSqlQuery resQuery(db);
        resQuery.prepare("INSERT INTO goal_resources (goal_id, type, value) VALUES (?, ?, ?)");
        resQuery.addBindValue(newGoalId);
        resQuery.addBindValue(res.startsWith("http") ? "link" : "file");
        resQuery.addBindValue(res);
        resQuery.exec();
    }
    if (recurrenceType != "None") {
        generateRecurringGoals(newGoalId);
    }
    emit goalCreated(newGoalId);
    return true;
}

bool StudyGoal::updateGoal(int goalId, const QString &subject, int targetMinutes, const QString &notes, const QString &recurrenceType, const QString &recurrenceValue, const QString &category, const QStringList &resources)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for updating goal.";
        return false;
    }
    // Check if the goal exists before updating
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM goals WHERE id = ?");
    checkQuery.addBindValue(goalId);
    checkQuery.exec();
    checkQuery.next();
    if (checkQuery.value(0).toInt() == 0) {
        qDebug() << "No goal found with ID" << goalId << "in database.";
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE goals SET subject = ?, target_minutes = ?, notes = ?, recurrence_type = ?, recurrence_value = ?, category = ? WHERE id = ?");
    query.addBindValue(subject);
    query.addBindValue(targetMinutes);
    query.addBindValue(notes);
    query.addBindValue(recurrenceType);
    query.addBindValue(recurrenceValue);
    query.addBindValue(category);
    query.addBindValue(goalId);
    if (!query.exec()) {
        qDebug() << "Error updating goal:" << query.lastError().text();
        return false;
    }
    // Remove old resources
    QSqlQuery delRes(db);
    delRes.prepare("DELETE FROM goal_resources WHERE goal_id = ?");
    delRes.addBindValue(goalId);
    delRes.exec();
    // Add new resources
    for (const QString &res : resources) {
        QSqlQuery resQuery(db);
        resQuery.prepare("INSERT INTO goal_resources (goal_id, type, value) VALUES (?, ?, ?)");
        resQuery.addBindValue(goalId);
        resQuery.addBindValue(res.startsWith("http") ? "link" : "file");
        resQuery.addBindValue(res);
        resQuery.exec();
    }
    emit goalUpdated(goalId);
    return true;
}

bool StudyGoal::deleteGoal(int goalId)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for deleting goal.";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM goals WHERE id = ?");
    query.addBindValue(goalId);

    if (!query.exec()) {
        qDebug() << "Error deleting goal:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() > 0) {
        qDebug() << "Goal" << goalId << "deleted successfully.";
        emit goalDeleted(goalId);
        return true;
    } else {
        qDebug() << "No goal found with ID" << goalId << "to delete.";
        return false;
    }
}

// Goal Retrieval
QList<GoalInfo> StudyGoal::getGoalsForDate(const QDate &date) const
{
    QList<GoalInfo> goals;
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for retrieving goals.";
        return goals;
    }
    QSqlQuery query(db);
    query.prepare("SELECT id, subject, target_minutes, completed_minutes, start_date, due_date, status, notes, recurrence_type, recurrence_value, last_generated_date, category FROM goals WHERE start_date <= ? AND (due_date IS NULL OR due_date >= ?) AND status = 'Active'");
    query.addBindValue(date.toString(Qt::ISODate));
    query.addBindValue(date.toString(Qt::ISODate));
    if (!query.exec()) {
        qDebug() << "Error retrieving goals for date:" << query.lastError().text();
        return goals;
    }
    while (query.next()) {
        GoalInfo goal;
        goal.id = query.value("id").toInt();
        goal.subject = query.value("subject").toString();
        goal.targetMinutes = query.value("target_minutes").toInt();
        goal.completedMinutes = query.value("completed_minutes").toInt();
        goal.startDate = query.value("start_date").toString();
        goal.dueDate = query.value("due_date").toString();
        goal.status = query.value("status").toString();
        goal.notes = query.value("notes").toString();
        goal.recurrenceType = query.value("recurrence_type").toString();
        goal.recurrenceValue = query.value("recurrence_value").toString();
        goal.lastGeneratedDate = QDate::fromString(query.value("last_generated_date").toString(), Qt::ISODate);
        goal.category = query.value("category").toString();
        // Fetch resources
        goal.resources = getResourcesForGoal(goal.id);
        goals.append(goal);
    }
    return goals;
}

QList<QMap<QString, QVariant>> StudyGoal::getGoalsForSubject(const QString &subject) const
{
    QList<QMap<QString, QVariant>> goals;
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for retrieving goals by subject.";
        return goals;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, subject, target_minutes, completed_minutes, notes, recurrence_type, recurrence_value, category FROM goals WHERE subject LIKE ? AND status = 'Active'");
    query.addBindValue("%" + subject + "%"); // Fuzzy search

    if (!query.exec()) {
        qDebug() << "Error retrieving goals by subject:" << query.lastError().text();
        return goals;
    }

    while (query.next()) {
        QMap<QString, QVariant> goal;
        goal["id"] = query.value("id").toInt();
        goal["subject"] = query.value("subject").toString();
        goal["target_minutes"] = query.value("target_minutes").toInt();
        goal["completed_minutes"] = query.value("completed_minutes").toInt();
        goal["notes"] = query.value("notes").toString();
        goal["recurrence_type"] = query.value("recurrence_type").toString();
        goal["recurrence_value"] = query.value("recurrence_value").toString();
        goal["category"] = query.value("category").toString();
        goals.append(goal);
    }
    return goals;
}

QMap<QString, QVariant> StudyGoal::getGoalDetails(int goalId) const
{
    QMap<QString, QVariant> details;
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for retrieving goal details.";
        return details;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, subject, target_minutes, completed_minutes, notes, recurrence_type, recurrence_value, category FROM goals WHERE id = ?");
    query.addBindValue(goalId);

    if (!query.exec()) {
        qDebug() << "Error retrieving goal details:" << query.lastError().text();
        return details;
    }

    if (query.next()) {
        details["id"] = query.value("id").toInt();
        details["subject"] = query.value("subject").toString();
        details["target_minutes"] = query.value("target_minutes").toInt();
        details["completed_minutes"] = query.value("completed_minutes").toInt();
        details["notes"] = query.value("notes").toString();
        details["recurrence_type"] = query.value("recurrence_type").toString();
        details["recurrence_value"] = query.value("recurrence_value").toString();
        details["category"] = query.value("category").toString();
    }
    return details;
}

// Progress Tracking
int StudyGoal::getProgress(int goalId) const
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for getting progress.";
        return 0;
    }
    QSqlQuery query(db);
    query.prepare("SELECT completed_minutes FROM goals WHERE id = ?");
    query.addBindValue(goalId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int StudyGoal::getTarget(int goalId) const
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for getting target.";
        return 0;
    }
    QSqlQuery query(db);
    query.prepare("SELECT target_minutes FROM goals WHERE id = ?");
    query.addBindValue(goalId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

float StudyGoal::getCompletionPercentage(int goalId) const
{
    int progress = getProgress(goalId);
    int target = getTarget(goalId);
    if (target > 0) {
        return (float)progress / target * 100.0f;
    }
    return 0.0f;
}

bool StudyGoal::addProgress(int goalId, int minutesToAdd)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for adding progress.";
        return false;
    }

    int currentCompletedMinutes = getProgress(goalId);
    int newCompletedMinutes = currentCompletedMinutes + minutesToAdd;

    QSqlQuery query(db);
    query.prepare("UPDATE goals SET completed_minutes = ? WHERE id = ?");
    query.addBindValue(newCompletedMinutes);
    query.addBindValue(goalId);

    if (!query.exec()) {
        qDebug() << "Error adding progress to goal:" << query.lastError().text();
        return false;
    }
    emit goalUpdated(goalId);
    emit sessionProgressUpdated(goalId, minutesToAdd); 
    return true;
}

void StudyGoal::connectToPomodoroTimer(PomodoroTimer *timer)
{
    m_pomodoroTimer = timer;
    if (m_pomodoroTimer) {
        connect(m_pomodoroTimer, &PomodoroTimer::timeUpdated, this, &StudyGoal::handlePomodoroTimeUpdated);
        connect(m_pomodoroTimer, &PomodoroTimer::timerCompleted, this, &StudyGoal::handlePomodoroTimerCompletedInternally);
    }
}

void StudyGoal::startTrackingGoal(int goalId, int initialElapsedSeconds)
{
    m_currentTrackingGoalId = goalId;
    m_pomodoroSessionStartTime = QTime::currentTime().addSecs(-initialElapsedSeconds);
    emit goalTrackingStarted(goalId);
}

void StudyGoal::stopTrackingGoal()
{
    if (m_currentTrackingGoalId != -1 && m_pomodoroTimer) {
        // Calculate elapsed time
        int elapsedSeconds = m_pomodoroSessionStartTime.secsTo(QTime::currentTime());
        int minutesToAdd = elapsedSeconds / 60;
        if (minutesToAdd > 0) {
            addProgress(m_currentTrackingGoalId, minutesToAdd);
            emit sessionProgressUpdated(m_currentTrackingGoalId, minutesToAdd);
        }
    }
    m_currentTrackingGoalId = -1;
    emit goalTrackingStopped();
}

void StudyGoal::handlePomodoroTimerCompletedInternally()
{
}

void StudyGoal::handlePomodoroTimeUpdated(const QString &time)
{
    Q_UNUSED(time);
    if (m_currentTrackingGoalId != -1) {
        int elapsedSeconds = m_pomodoroSessionStartTime.secsTo(QTime::currentTime());
        int completedMinutesFromDB = getProgress(m_currentTrackingGoalId);
        int targetMinutes = getTarget(m_currentTrackingGoalId);
        int sessionMinutes = elapsedSeconds / 60;
        int totalCompleted = completedMinutesFromDB + sessionMinutes;
        if (totalCompleted >= targetMinutes && targetMinutes > 0) {
            emit sessionTimeUpdated(m_currentTrackingGoalId, (targetMinutes - completedMinutesFromDB) * 60);
            stopTrackingGoal();
            return;
        }
        emit sessionTimeUpdated(m_currentTrackingGoalId, elapsedSeconds);
    }
}

// Recurring Goals
void StudyGoal::generateRecurringGoals(int parentGoalId)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for generating recurring goals.";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, subject, target_minutes, notes, recurrence_type, recurrence_value, last_generated_date, category FROM goals WHERE id = ?");
    query.addBindValue(parentGoalId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Error retrieving parent goal for recurrence:" << query.lastError().text();
        return;
    }

    GoalInfo parentGoal;
    parentGoal.id = query.value("id").toInt();
    parentGoal.subject = query.value("subject").toString();
    parentGoal.targetMinutes = query.value("target_minutes").toInt();
    parentGoal.notes = query.value("notes").toString();
    parentGoal.recurrenceType = query.value("recurrence_type").toString();
    parentGoal.recurrenceValue = query.value("recurrence_value").toString();
    parentGoal.lastGeneratedDate = QDate::fromString(query.value("last_generated_date").toString(), Qt::ISODate);
    parentGoal.category = query.value("category").toString();

    QDate today = QDate::currentDate();
    QDate nextGenerationDate = parentGoal.lastGeneratedDate;

    if (parentGoal.recurrenceType == "Daily") {
        nextGenerationDate = parentGoal.lastGeneratedDate.addDays(1);
    } else if (parentGoal.recurrenceType == "Weekly") {
        QStringList days = parentGoal.recurrenceValue.split(',', Qt::SkipEmptyParts);
        QMap<QString, int> dayMap;
        dayMap["Mon"] = 1; dayMap["Tue"] = 2; dayMap["Wed"] = 3; dayMap["Thu"] = 4;
        dayMap["Fri"] = 5; dayMap["Sat"] = 6; dayMap["Sun"] = 7;

        bool foundNext = false;
        for (int i = 0; i < 8; ++i) {
            QDate checkDate = parentGoal.lastGeneratedDate.addDays(i + 1);
            if (days.contains(checkDate.toString("ddd"))) {
                nextGenerationDate = checkDate;
                foundNext = true;
                break;
            }
        }
        if (!foundNext) {
            nextGenerationDate = parentGoal.lastGeneratedDate.addDays(7);
        }
    } else if (parentGoal.recurrenceType == "Monthly") {
        QStringList days = parentGoal.recurrenceValue.split(',', Qt::SkipEmptyParts);
        bool foundNext = false;
        for (int i = 0; i < 32; ++i) {
            QDate checkDate = parentGoal.lastGeneratedDate.addDays(i + 1);
            if (days.contains(QString::number(checkDate.day()))) {
                nextGenerationDate = checkDate;
                foundNext = true;
                break;
            }
        }
        if (!foundNext) {
            nextGenerationDate = QDate(nextGenerationDate.year(), nextGenerationDate.month() + 1, 1);
        }
    } else {
        return;
    }

    while (nextGenerationDate <= today) {
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO goals (subject, target_minutes, notes, start_date, recurrence_type, recurrence_value, last_generated_date, category, due_date) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insertQuery.addBindValue(parentGoal.subject);
        insertQuery.addBindValue(parentGoal.targetMinutes);
        insertQuery.addBindValue(parentGoal.notes);
        insertQuery.addBindValue(nextGenerationDate.toString(Qt::ISODate));
        insertQuery.addBindValue("None");
        insertQuery.addBindValue("");
        insertQuery.addBindValue(nextGenerationDate.toString(Qt::ISODate));
        insertQuery.addBindValue(parentGoal.category);
        insertQuery.addBindValue(nextGenerationDate.toString(Qt::ISODate));

        if (!insertQuery.exec()) {
            qDebug() << "Error creating recurring goal instance:" << insertQuery.lastError().text();
        } else {
            qDebug() << "Generated recurring goal for" << nextGenerationDate.toString(Qt::ISODate);
            emit goalCreated(insertQuery.lastInsertId().toInt());
        }

        // Update the last_generated_date of the parent goal
        QSqlQuery updateParentQuery(db);
        updateParentQuery.prepare("UPDATE goals SET last_generated_date = ? WHERE id = ?");
        updateParentQuery.addBindValue(nextGenerationDate.toString(Qt::ISODate));
        updateParentQuery.addBindValue(parentGoal.id);
        if (!updateParentQuery.exec()) {
            qDebug() << "Error updating parent goal's last_generated_date:" << updateParentQuery.lastError().text();
        }

        // Move to the next potential generation date for the loop
        if (parentGoal.recurrenceType == "Daily") {
            nextGenerationDate = nextGenerationDate.addDays(1);
        } else if (parentGoal.recurrenceType == "Weekly") {
            nextGenerationDate = nextGenerationDate.addDays(1);
        } else if (parentGoal.recurrenceType == "Monthly") {
            nextGenerationDate = nextGenerationDate.addMonths(1);
        } else {
            break;
        }
    }
}

void StudyGoal::checkAndGenerateAllRecurringGoals() {
    if (!db.isOpen()) return;
    QSqlQuery query(db);
    query.prepare("SELECT id, recurrence_type, last_generated_date FROM goals WHERE recurrence_type != 'None'");
    if (!query.exec()) return;
    QDate today = QDate::currentDate();
    while (query.next()) {
        int goalId = query.value(0).toInt();
        QString recurrenceType = query.value(1).toString();
        QDate lastGen = QDate::fromString(query.value(2).toString(), Qt::ISODate);
        if (lastGen.isValid() && lastGen < today) {
            generateRecurringGoals(goalId);
        }
    }
}

// Statistics
QMap<QString, int> StudyGoal::getSubjectStats(const QDate &startDate, const QDate &endDate)
{
    QMap<QString, int> stats;
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for getting subject stats.";
        return stats;
    }

    QSqlQuery query(db);
    query.prepare("SELECT subject, SUM(completed_minutes) FROM goals WHERE start_date BETWEEN ? AND ? GROUP BY subject");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));

    if (!query.exec()) {
        qDebug() << "Error getting subject stats:" << query.lastError().text();
        return stats;
    }

    while (query.next()) {
        stats[query.value(0).toString()] = query.value(1).toInt();
    }
    return stats;
}

QMap<QString, int> StudyGoal::getDailyStats(const QDate &date)
{
    QMap<QString, int> stats;
    if (!db.isOpen()) {
        qDebug() << "Error: Database not open for getting daily stats.";
        return stats;
    }

    QSqlQuery query(db);
    query.prepare("SELECT SUM(completed_minutes) FROM goals WHERE start_date = ?");
    query.addBindValue(date.toString(Qt::ISODate));

    if (!query.exec()) {
        qDebug() << "Error getting daily stats:" << query.lastError().text();
        return stats;
    }

    if (query.next()) {
        stats["totalMinutes"] = query.value(0).toInt();
    } else {
        stats["totalMinutes"] = 0;
    }
    return stats;
}

QList<QString> StudyGoal::getResourcesForGoal(int goalId) const
{
    QList<QString> resources;
    if (!db.isOpen()) return resources;
    QSqlQuery query(db);
    query.prepare("SELECT value FROM goal_resources WHERE goal_id = ?");
    query.addBindValue(goalId);
    if (query.exec()) {
        while (query.next()) {
            resources.append(query.value(0).toString());
        }
    }
    return resources;
}
