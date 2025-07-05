#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QCategoryAxis>
#include <QDateTimeAxis>

class StudyAnalytics : public QObject
{
    Q_OBJECT

public:
    explicit StudyAnalytics(QSqlDatabase &database, QObject *parent = nullptr);
    ~StudyAnalytics();

    // Chart Generation
    QChart* createFocusChart(const QDate &startDate, const QDate &endDate);
    QChart* createProductivityChart(const QDate &startDate, const QDate &endDate);
    QChart* createStreakChart(const QDate &startDate, const QDate &endDate);
    QChart* createSubjectDistributionChart(const QDate &startDate, const QDate &endDate);
    QChart* createEffectivenessChart(const QDate &startDate, const QDate &endDate);
    QChart* createDistractionChart(const QDate &startDate, const QDate &endDate);
    QChart* createGoalCompletionChart(const QDate &startDate, const QDate &endDate);
    
    // Data Analysis
    QMap<QString, float> getFocusPatterns() const;
    QMap<QString, int> getProductivityByHour() const;
    QMap<QString, int> getProductivityByDay() const;
    QMap<QString, float> getSubjectPerformance() const;
    
    // Statistics
    float getAverageFocusScore() const;
    int getMostProductiveHour() const;
    QString getMostProductiveDay() const;
    QString getBestPerformingSubject() const;
    
    // Predictions
    QMap<QString, float> predictOptimalStudyTimes() const;
    float predictNextSessionFocus() const;
    int predictNextNextSessionDuration() const;

public slots:
    void analyzePatterns(const QDate &startDate, const QDate &endDate);

signals:
    void analyticsUpdated();
    void patternDetected(const QString &pattern);
    void productivityInsight(const QString &insight);

private:
    QSqlDatabase &db;
    QMap<QString, QList<float>> focusHistory;
    QMap<QString, QList<int>> productivityHistory;
    
    bool initializeDatabase();
    void loadHistoricalData(const QDate &startDate, const QDate &endDate);
    QList<float> calculateMovingAverage(const QList<float> &data, int window) const;
    float calculateCorrelation(const QList<float> &x, const QList<float> &y) const;
    QString formatTimeRange(int hour) const;
    QString formatDayOfWeek(int day) const;
};

#endif // ANALYTICS_H 