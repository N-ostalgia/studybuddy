#ifndef SURVEY_H
#define SURVEY_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QSqlDatabase>

struct SurveyResult {
    int goalId;
    QDateTime timestamp;
    QString moodEmoji;
    int distractionLevel; // 0-100
    QList<QString> distractions;
    int sessionSatisfaction = 0; // 1-5 stars
    QString goalAchieved; // "Yes", "No", "Partial"
    QString openFeedback;
    bool setReminder = false;
};

class Survey : public QObject {
    Q_OBJECT
public:
    explicit Survey(QSqlDatabase& db, QObject *parent = nullptr);
    ~Survey();
    QList<SurveyResult> getSurveyResultsForGoal(int goalId) const;
    bool saveSurveyResult(const SurveyResult &result);
    QList<SurveyResult> getAllSurveyResults() const;
    QList<QString> getCommonDistractions() const;
    bool deleteSurveyResult(int goalId, const QDateTime &timestamp);
private:
    QSqlDatabase& db;
};

#endif // SURVEY_H 