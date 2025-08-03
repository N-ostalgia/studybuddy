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
#include <QtCharts/QLegendMarker>
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
    series->setColor(QColor("#C3073F")); // Carmine red
    QSqlQuery query(db);
    query.prepare("SELECT date(timestamp) as day, AVG(focus_score) as avg_focus FROM detections WHERE date(timestamp) BETWEEN ? AND ? GROUP BY day ORDER BY day");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        qDebug() << "Focus chart query failed:" << query.lastError().text();
    }
    int count = 0;
    while (query.next()) {
        QString day = query.value(0).toString();
        float avgFocus = query.value(1).toFloat();
        qDebug() << "FocusChart day:" << day << "avgFocus:" << avgFocus;
        QDate date = QDate::fromString(day, "yyyy-MM-dd");
        series->append(QDateTime(date, QTime(0,0)).toMSecsSinceEpoch(), avgFocus * 100); // percent
        count++;
    }
    if (count == 0) qDebug() << "Focus chart: no data for selected range.";
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
    // Force legend color
    QPen pen(QColor("#C3073F"));
    pen.setWidth(3);
    series->setPen(pen);
    return chart;
}

QChart* StudyAnalytics::createProductivityChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    QLineSeries *series = new QLineSeries();
    series->setColor(QColor("#C3073F")); // Carmine red
    QMap<QDate, int> detectionsPerDay;
    QSqlQuery query(db);
    query.prepare("SELECT date(timestamp) as study_date, COUNT(*) as detections_count FROM detections WHERE date(timestamp) BETWEEN ? AND ? GROUP BY study_date ORDER BY study_date");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        qDebug() << "Productivity chart query failed:" << query.lastError().text();
    }
    while (query.next()) {
        QString day = query.value(0).toString();
        int detectionsCount = query.value(1).toInt();
        qDebug() << "ProductivityChart day:" << day << "detectionsCount:" << detectionsCount;
        QDate date = QDate::fromString(day, "yyyy-MM-dd");
        detectionsPerDay[date] = detectionsCount;
    }
    // Fill in zeros for days with no data
    QDate d = startDate;
    while (d <= endDate) {
        int count = detectionsPerDay.value(d, 0);
        series->append(QDateTime(d, QTime(0, 0)).toMSecsSinceEpoch(), count);
        d = d.addDays(1);
    }
    if (series->count() == 0) qDebug() << "Productivity chart: no data for selected range.";
    chart->addSeries(series);
    chart->setTitle("Study Detections Over Time");
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("MMM dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 500); // You may want to adjust this dynamically
    axisY->setTitleText("Detections Count");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->setTheme(QChart::ChartThemeDark);
    // Force legend color and marker style
    QPen pen(QColor("#C3073F"));
    pen.setWidth(3);
    series->setPen(pen);
    series->setPointsVisible(true);
    series->setMarkerSize(5.0); // Make dots smaller
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

QStringList StudyAnalytics::getNeglectedSubjects(int days, const QDate &endDate) {
    QStringList neglected;
    QSqlQuery query(db);
    // Get all subjects
    query.prepare("SELECT DISTINCT subject FROM goals");
    if (!query.exec()) return neglected;
    QList<QString> subjects;
    while (query.next()) {
        subjects << query.value(0).toString();
    }
    QDate startDate = endDate.addDays(-days+1);
    for (const QString& subject : subjects) {
        QSqlQuery q(db);
        q.prepare("SELECT MAX(start_date) FROM goals WHERE subject = ? AND completed_minutes > 0");
        q.addBindValue(subject);
        if (q.exec() && q.next()) {
            QString lastDateStr = q.value(0).toString();
            if (lastDateStr.isEmpty()) {
                neglected << subject;
            } else {
                QDate lastDate = QDate::fromString(lastDateStr, "yyyy-MM-dd");
                if (lastDate.isValid() && lastDate < startDate) {
                    neglected << subject;
                }
            }
        }
    }
    return neglected;
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

    // Neglected subjects insight
    QStringList neglected = getNeglectedSubjects(7, endDate); // 7 days
    if (!neglected.isEmpty()) {
        emit patternDetected(QString("You haven't studied %1 in the last 7 days.").arg(neglected.join(", ")));
    }

    // Focus drop-off insight
    QMap<int, QList<float>> focusByMinute; // minute -> focus scores
    QSqlQuery query(db);
    query.prepare("SELECT timestamp, focus_score FROM detections WHERE date(timestamp) BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (query.exec()) {
        QMap<QString, QTime> sessionStartTimes; // date string -> first time
        while (query.next()) {
            QString ts = query.value(0).toString();
            float focus = query.value(1).toFloat();
            QDateTime dt = QDateTime::fromString(ts, "yyyy-MM-dd HH:mm:ss");
            QString dateStr = dt.date().toString("yyyy-MM-dd");
            if (!sessionStartTimes.contains(dateStr)) {
                sessionStartTimes[dateStr] = dt.time();
            }
            int minute = sessionStartTimes[dateStr].secsTo(dt.time()) / 60;
            if (minute >= 0 && minute < 180) // up to 3 hours
                focusByMinute[minute].append(focus);
        }
        QMap<int, float> avgFocusByMinute;
        for (auto it = focusByMinute.begin(); it != focusByMinute.end(); ++it) {
            float sum = 0;
            for (float f : it.value()) sum += f;
            avgFocusByMinute[it.key()] = it.value().isEmpty() ? 0 : sum / it.value().size();
        }
        float base = 0;
        int baseCount = 0;
        for (int i = 0; i < 10; ++i) {
            if (avgFocusByMinute.contains(i)) {
                base += avgFocusByMinute[i];
                baseCount++;
            }
        }
        if (baseCount > 0) {
            float baseAvg = base / baseCount;
            for (int i = 10; i < 120; ++i) {
                if (avgFocusByMinute.contains(i) && avgFocusByMinute[i] < baseAvg * 0.9) {
                    emit patternDetected(QString("Your focus drops after %1 minutes. Try shorter sessions for better results.").arg(i));
                    break;
                }
            }
        }
    }

    // Distraction pattern insight
    QMap<QString, QMap<QString, int>> distractionByDay; // distraction -> day -> count
    QSqlQuery surveyQ(db);
    surveyQ.prepare("SELECT timestamp, distractions FROM surveys WHERE timestamp BETWEEN ? AND ?");
    surveyQ.addBindValue(startDate.toString("yyyy-MM-dd"));
    surveyQ.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (surveyQ.exec()) {
        while (surveyQ.next()) {
            QString ts = surveyQ.value(0).toString();
            QString distractions = surveyQ.value(1).toString();
            QDateTime dt = QDateTime::fromString(ts, "yyyy-MM-ddTHH:mm:ss");
            if (!dt.isValid()) dt = QDateTime::fromString(ts, "yyyy-MM-dd HH:mm:ss");
            QString day = dt.date().toString("dddd");
            for (const QString& d : distractions.split(",", Qt::SkipEmptyParts)) {
                QString trimmed = d.trimmed();
                if (!trimmed.isEmpty())
                    distractionByDay[trimmed][day]++;
            }
        }
        QString topDistraction;
        QString topDay;
        int maxCount = 0;
        for (auto it = distractionByDay.begin(); it != distractionByDay.end(); ++it) {
            for (auto dayIt = it.value().begin(); dayIt != it.value().end(); ++dayIt) {
                if (dayIt.value() > maxCount) {
                    maxCount = dayIt.value();
                    topDistraction = it.key();
                    topDay = dayIt.key();
                }
            }
        }
        if (!topDistraction.isEmpty() && !topDay.isEmpty()) {
            emit patternDetected(QString("You report '%1' as a distraction most on %2.").arg(topDistraction, topDay));
        }
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

QChart* StudyAnalytics::createGoalCompletionChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTheme(QChart::ChartThemeDark); 
    chart->setTitle("Goal Completion Rate");
    QPieSeries *series = new QPieSeries();
    int completed = 0, incomplete = 0;
    QSqlQuery query(db);
    query.prepare("SELECT completed_minutes, target_minutes FROM goals WHERE start_date BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        qDebug() << "Goal completion chart query failed:" << query.lastError().text();
    }
    while (query.next()) {
        int done = query.value(0).toInt();
        int target = query.value(1).toInt();
        if (target > 0 && done >= target)
            completed++;
        else
            incomplete++;
    }
    if (completed > 0)
        series->append("Completed", completed);
    if (incomplete > 0)
        series->append("Incomplete", incomplete);

    // Set colors explicitly for all slices and legend markers
    for (auto slice : series->slices()) {
        if (slice->label() == "Completed") {
            slice->setColor(QColor("#C3073F"));
            slice->setBrush(QColor("#C3073F"));
            slice->setPen(QPen(QColor("#C3073F")));
        } else {
            slice->setColor(QColor("#57071f"));
            slice->setBrush(QColor("#57071f"));
            slice->setPen(QPen(QColor("#57071f")));
        }
        slice->setBorderColor(slice->color());
    }
    chart->addSeries(series);

    // Force legend marker color
    auto markers = chart->legend()->markers(series);
    for (int i = 0; i < series->count(); ++i) {
        QPieSlice *slice = series->slices().at(i);
        if (i < markers.size()) {
            markers[i]->setBrush(slice->brush());
            markers[i]->setPen(slice->pen());
        }
    }
    return chart;
} 

QChart* StudyAnalytics::createSubjectTimeChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTheme(QChart::ChartThemeDark);
    chart->setTitle("Time Spent per Subject");
    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Minutes");
    set->setColor(QColor("#C3073F"));
    set->setBrush(QColor("#C3073F"));
    QStringList categories;
    QMap<QString, int> subjectMinutes;
    QSqlQuery query(db);
    query.prepare("SELECT subject, SUM(completed_minutes) FROM goals WHERE start_date BETWEEN ? AND ? GROUP BY subject");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        qDebug() << "Subject time chart query failed:" << query.lastError().text();
    }
    while (query.next()) {
        QString subject = query.value(0).toString();
        int minutes = query.value(1).toInt();
        categories << subject;
        *set << minutes;
    }
    if (categories.isEmpty()) {
        categories << "No Data";
        *set << 0;
    }
    series->append(set);
    chart->addSeries(series);
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Minutes");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    return chart;
} 

QChart* StudyAnalytics::createDistractionBarChart(const QDate &startDate, const QDate &endDate)
{
    QChart *chart = new QChart();
    chart->setTheme(QChart::ChartThemeDark);
    chart->setTitle("Reported Distractions");
    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Count");
    set->setColor(QColor("#C3073F"));
    set->setBrush(QColor("#C3073F"));
    QStringList categories;
    QMap<QString, int> distractionCounts;
    QSqlQuery query(db);
    query.prepare("SELECT distractions FROM surveys WHERE timestamp BETWEEN ? AND ?");
    query.addBindValue(startDate.toString("yyyy-MM-dd"));
    query.addBindValue(endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        qDebug() << "Distraction bar chart query failed:" << query.lastError().text();
    }
    while (query.next()) {
        QString distractions = query.value(0).toString();
        for (const QString& d : distractions.split(",", Qt::SkipEmptyParts)) {
            QString trimmed = d.trimmed();
            if (!trimmed.isEmpty())
                distractionCounts[trimmed]++;
        }
    }
    for (auto it = distractionCounts.begin(); it != distractionCounts.end(); ++it) {
        categories << it.key();
        *set << it.value();
    }
    if (categories.isEmpty()) {
        categories << "No Data";
        *set << 0;
    }
    series->append(set);
    chart->addSeries(series);
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Count");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    return chart;
} 
