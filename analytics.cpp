#include "analytics.h"
#include <QSqlError>
#include <QDebug>
#include <QtMath>
#include <QDateTime>
#include <QSqlRecord>
#include <QDateTimeAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QColor>

StudyAnalytics::StudyAnalytics(QSqlDatabase &database, QObject *parent)
    : QObject(parent),
      db(database)
{
}

StudyAnalytics::~StudyAnalytics()
{
}

void StudyAnalytics::loadHistoricalData(const QDate &startDate, const QDate &endDate)
{
    focusHistory.clear();
    productivityHistory.clear();

    QSqlQuery query(db);
    query.prepare("SELECT timestamp, focus_score FROM detections "
                  "WHERE date(timestamp) BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    
    while (query.next()) {
        QDateTime timestamp = query.value(0).toDateTime();
        float focusScore = query.value(1).toFloat();
        
        QString hourKey = formatTimeRange(timestamp.time().hour());
        productivityHistory[QString::number(timestamp.time().hour())].append(1); 
        
        focusHistory[hourKey].append(focusScore);
    }
}

QChart* StudyAnalytics::createFocusChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    QLineSeries *series = new QLineSeries();
    series->setColor(QColor("#6F2232"));
    QSqlQuery query(db);
    query.prepare("SELECT timestamp, focus_score FROM detections "
                 "WHERE date(timestamp) BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        QDateTime time = query.value(0).toDateTime();
        float score = query.value(1).toFloat();
        series->append(time.toMSecsSinceEpoch(), score);
    }
    chart->addSeries(series);
    chart->setTitle("Focus Score Over Time");
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("MMM dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setTitleText("Focus Score");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QChart* StudyAnalytics::createProductivityChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    QLineSeries *series = new QLineSeries();
    series->setColor(QColor("#6F2232"));
    QSqlQuery query(db);
    query.prepare("SELECT date(timestamp) as study_date, COUNT(*) as detections_count FROM detections "
                 "WHERE date(timestamp) BETWEEN ? AND ? GROUP BY study_date ORDER BY study_date");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        QDate date = query.value(0).toDate();
        int detectionsCount = query.value(1).toInt();
        series->append(QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch(), detectionsCount); 
    }
    chart->addSeries(series);
    chart->setTitle("Study Detections Over Time"); 
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("MMM dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 500); 
    axisY->setTitleText("Detections Count"); 
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QMap<QString, float> StudyAnalytics::getFocusPatterns() const
{
    QMap<QString, float> patterns;

    for (auto it = focusHistory.begin(); it != focusHistory.end(); ++it) {
        float sum = 0;
        for (float score : it.value()) {
            sum += score;
        }
        patterns[it.key()] = sum / it.value().size();
    }
    return patterns;
}

float StudyAnalytics::getAverageFocusScore() const
{
    float total = 0;
    int count = 0;
    
    for (const auto &scores : focusHistory) {
        for (float score : scores) {
            total += score;
            count++;
        }
    }
    
    return count > 0 ? total / count : 0;
}

int StudyAnalytics::getMostProductiveHour() const
{
    int maxProductivity = 0;
    int bestHour = 0;
    
    for (auto it = productivityHistory.begin(); it != productivityHistory.end(); ++it) {
        int total = 0;
        for (int duration : it.value()) {
            total += duration;
        }
        int currentHour = it.key().toInt();
        if (total > maxProductivity) {
            maxProductivity = total;
            bestHour = currentHour;
        }
    }
    
    return bestHour;
}

QMap<QString, float> StudyAnalytics::predictOptimalStudyTimes() const
{
    QMap<QString, float> predictions;
    QMap<QString, float> focusPatterns = getFocusPatterns();
    
    // Simple prediction based on historical focus scores
    for (auto it = focusPatterns.begin(); it != focusPatterns.end(); ++it) {
        if (it.value() > 70) {
            predictions[it.key()] = it.value();
        }
    }
    
    return predictions;
}

QList<float> StudyAnalytics::calculateMovingAverage(const QList<float> &data, int window) const
{
    QList<float> result;
    for (int i = 0; i <= data.size() - window; ++i) {
        float sum = 0;
        for (int j = 0; j < window; ++j) {
            sum += data[i + j];
        }
        result.append(sum / window);
    }
    return result;
}

float StudyAnalytics::calculateCorrelation(const QList<float> &x, const QList<float> &y) const
{
    if (x.size() != y.size() || x.isEmpty()) {
        return 0;
    }
    
    float sumX = 0, sumY = 0, sumXY = 0;
    float sumX2 = 0, sumY2 = 0;
    int n = x.size();
    
    for (int i = 0; i < n; ++i) {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
        sumY2 += y[i] * y[i];
    }
    
    float numerator = n * sumXY - sumX * sumY;
    float denominator = qSqrt((n * sumX2 - sumX * sumX) * (n * sumY2 - sumY * sumY));
    
    return denominator != 0 ? numerator / denominator : 0;
}

QString StudyAnalytics::formatTimeRange(int hour) const
{
    return QString("%1:00-%2:00").arg(hour, 2, 10, QChar('0')).arg(hour + 1, 2, 10, QChar('0'));
}

QString StudyAnalytics::formatDayOfWeek(int day) const
{
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    return QString(days[day - 1]);
}

void StudyAnalytics::analyzePatterns(const QDate &startDate, const QDate &endDate)
{
    loadHistoricalData(startDate, endDate);
    // Analyze focus patterns by hour
    QMap<QString, float> hourlyFocus = getFocusPatterns();
    for (auto it = hourlyFocus.begin(); it != hourlyFocus.end(); ++it) {
        if (it.value() > 80) {
            emit patternDetected(QString("High focus detected during %1").arg(it.key()));
        }
    }

    // Analyze productivity patterns
    QMap<QString, int> hourlyProductivity = getProductivityByHour();
    int mostProductiveHour = getMostProductiveHour();
    if (mostProductiveHour > 0) {
        emit productivityInsight(QString("Most productive hour: %1:00").arg(mostProductiveHour));
    }

    // Analyze subject performance
    QMap<QString, float> subjectPerformance = getSubjectPerformance();
    QString bestSubject = getBestPerformingSubject();
    if (!bestSubject.isEmpty()) {
        emit productivityInsight(QString("Best performing subject: %1").arg(bestSubject));
    }

    emit analyticsUpdated();
}

QMap<QString, int> StudyAnalytics::getProductivityByHour() const
{
    QMap<QString, int> hourlyProductivity;
    QSqlQuery query(db);
    
    query.exec("SELECT strftime('%H', timestamp) as hour, COUNT(*) as detections_count "
              "FROM detections GROUP BY hour ORDER BY hour");
    
    while (query.next()) {
        int hour = query.value(0).toInt();
        int detectionsCount = query.value(1).toInt();
        hourlyProductivity[formatTimeRange(hour)] = detectionsCount;
    }
    
    return hourlyProductivity;
}

QMap<QString, float> StudyAnalytics::getSubjectPerformance() const
{
    qDebug() << "Subject performance analysis not available with current database schema (no subject field in 'detections' table).";
    return QMap<QString, float>();
}

QString StudyAnalytics::getBestPerformingSubject() const
{
    return QString();
}

QChart* StudyAnalytics::createEffectivenessChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTitle("Session Effectiveness Over Time");
    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Effectiveness");
    set->setColor(QColor("#C3073F"));
    QStringList categories;
    QMap<QString, int> effectivenessCounts;
    QSqlQuery query(db);
    query.prepare("SELECT effectiveness, COUNT(*) FROM session_reviews WHERE date(review_date) BETWEEN ? AND ? GROUP BY effectiveness");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        QString eff = query.value(0).toString();
        int count = query.value(1).toInt();
        effectivenessCounts[eff] = count;
    }
    QStringList effOrder = {"Highly Effective", "Moderately Effective", "Neutral", "Slightly Ineffective", "Highly Ineffective"};
    for (const QString &eff : effOrder) {
        categories << eff;
        *set << effectivenessCounts.value(eff, 0);
    }
    series->append(set);
    chart->addSeries(series);
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Sessions");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QChart* StudyAnalytics::createDistractionChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTitle("Distraction Events Frequency");
    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Distraction Events");
    set->setColor(QColor("#C3073F"));
    QStringList categories;
    QMap<QString, int> distractionCounts;
    QSqlQuery query(db);
    query.prepare("SELECT distraction_events, COUNT(*) FROM session_reviews WHERE date(review_date) BETWEEN ? AND ? GROUP BY distraction_events");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    int withDistraction = 0, withoutDistraction = 0;
    while (query.next()) {
        QString events = query.value(0).toString().trimmed();
        int count = query.value(1).toInt();
        if (!events.isEmpty())
            withDistraction += count;
        else
            withoutDistraction += count;
    }
    categories << "With Distraction" << "No Distraction";
    *set << withDistraction << withoutDistraction;
    series->append(set);
    chart->addSeries(series);
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Sessions");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QChart* StudyAnalytics::createSubjectDistributionChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTitle("Most Studied Subjects");
    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Sessions");
    set->setColor(QColor("#C3073F"));
    QStringList categories;
    QSqlQuery query(db);
    query.prepare("SELECT subject, COUNT(*) FROM session_plans WHERE date(plan_date) BETWEEN ? AND ? GROUP BY subject ORDER BY COUNT(*) DESC LIMIT 10");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        QString subject = query.value(0).toString();
        int count = query.value(1).toInt();
        categories << subject;
        *set << count;
    }
    series->append(set);
    chart->addSeries(series);
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Sessions");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QChart* StudyAnalytics::createGoalCompletionChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTitle("Goal Completion Rate");
    QPieSeries *series = new QPieSeries();
    int completed = 0, incomplete = 0;
    QSqlQuery query(db);
    query.prepare("SELECT completed_minutes, target_minutes FROM goals WHERE start_date BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        int done = query.value(0).toInt();
        int target = query.value(1).toInt();
        if (target > 0 && done >= target)
            completed++;
        else
            incomplete++;
    }
    series->append("Completed", completed);
    series->append("Incomplete", incomplete);
    if (!series->slices().isEmpty())
        series->slices().at(0)->setColor(QColor("#6F2232"));
    if (series->slices().size() > 1)
        series->slices().at(1)->setColor(QColor("#4E4E50"));
    chart->addSeries(series);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
}

QChart* StudyAnalytics::createStreakChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTitle("Streak Progress");
    QLineSeries *series = new QLineSeries();
    series->setColor(QColor("#6F2232"));
    QSqlQuery query(db);
    query.prepare("SELECT date, streak_count FROM study_streaks WHERE date BETWEEN ? AND ? ORDER BY date");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    query.exec();
    while (query.next()) {
        QDate date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
        int streak = query.value(1).toInt();
        series->append(QDateTime(date, QTime(0,0)).toMSecsSinceEpoch(), streak);
    }
    chart->addSeries(series);
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("MMM dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Streak Count");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    return chart;
} 
