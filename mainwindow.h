#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QtMultimedia/QSoundEffect>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QToolBar>
#include <QSettings>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QtCharts>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <QScrollArea>
#include "pomodoro.h"
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include "studygoals.h"
#include "analytics.h"
#include "achievements.h"
#include "achievementitemdelegate.h"
#include <QMap>
#include <QComboBox>
#include "streak.h"
#include "studysession.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startWebcam();
    void stopWebcam();
    void updateFrame();
    void showDetectionHistory();
    void exportData();
    void showSettings();
    void showFocusAlert();
    void updateDashboard();

    // Pomodoro Slots
    void updatePomodoroDisplay(const QString &time);
    void handlePomodoroModeChanged(bool isStudyTime);
    void handlePomodoroCycleChanged(int cycle);
    void handlePomodoroTimerCompleted();

    // Study Goal Slots
    void handleAddGoal();
    void handleDeleteGoal();
    void handleStartGoalTracking();
    void handleStopGoalTracking();
    void refreshGoalsDisplay();
    void handleGoalCreated(int goalId);
    void handleGoalDeleted(int goalId);
    void handleGoalUpdated(int goalId);
    void onGoalSessionTimeUpdated(int goalId, int elapsedSeconds);
    void onGoalSessionProgressUpdated(int goalId, int sessionMinutes);
    void onGoalTrackingStarted(int goalId);
    void onGoalTrackingStopped();

    // Achievement Slots
    void onAchievementUnlocked(const QString &achievementId, const QString &name);
    void onAchievementProgressUpdated(const QString &achievementId, int progress);
    void refreshAchievementsDisplay();
    void handleCancelEdit();

private:
    Ui::MainWindow *ui;

    // UI Elements
    QTabWidget *tabWidget;
    QLabel *webcamLabel;
    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *showHistoryButton;
    QPushButton *exportButton;
    QPushButton *settingsButton;
    QCheckBox *enableFaceDetectionCheckbox;
    QTimer *timer;
    QLabel *blinkCountLabel;
    QProgressBar *focusProgressBar;
    QSystemTrayIcon *trayIcon;
    QLabel *sessionDurationLabel;
    QLabel *focusScoreLabel;
    QSoundEffect *alertSound;

    // Pomodoro Timer Elements
    QLabel *pomodoroTimeLabel;
    QLabel *pomodoroModeLabel;
    QLabel *pomodoroCycleLabel;
    QPushButton *pomodoroStartButton;
    QPushButton *pomodoroPauseButton;
    QPushButton *pomodoroResetButton;
    QPushButton *pomodoroSkipButton;
    PomodoroTimer *pomodoroTimer;

    // Study Goal Elements
    QListWidget *goalsListWidget;
    QLineEdit *goalSubjectInput;
    QSpinBox *goalTargetMinutesInput;
    QTextEdit *goalNotesInput;
    QPushButton *addGoalButton;
    QPushButton *deleteGoalButton;
    QPushButton *startTrackingButton;
    QPushButton *stopTrackingButton;
    StudyGoal *studyGoal;
    QComboBox *goalCategoryCombo;
    QPushButton *cancelEditButton;
    QComboBox *recurrenceTypeCombo;
    QLineEdit *recurrenceValueInput;
    // Achievement Elements
    QLabel *totalAchievementsLabel;
    QLabel *unlockedAchievementsLabel;
    QLabel *achievementCompletionPercentageLabel;
    Achievement *achievementTracker;
    QLabel *m_achievementDescriptionLabel;

    // Chart Elements
    QChartView *focusChartView;
    QChartView *blinkChartView;
    QLineSeries *focusSeries;
    QLineSeries *blinkSeries;
    QValueAxis *timeAxis;
    QValueAxis *focusAxis;
    QValueAxis *blinkAxis;
    QChart *focusChart;
    QChart *blinkChart;

    // OpenCV Elements
    cv::VideoCapture capture;
    cv::Mat frame;
    cv::CascadeClassifier faceCascade;
    cv::CascadeClassifier eyeCascade;

    // State Variables
    bool webcamActive;
    bool faceDetectionEnabled;
    bool alertsEnabled;
    int alertThreshold;
    QString soundFile;

    // Detection Variables
    bool eyesPreviouslyDetected;
    int blinkCount;
    double blinkRate;
    int consecutiveNoEyeFrames;
    int totalFrames;
    int focusedFrames;
    QElapsedTimer sessionTimer;
    float currentFocusScore;
    QDateTime sessionStartTime;

    // Database
    QSqlDatabase db;

    // Tracking current active goal
    int m_currentActiveGoalId;
    int m_currentSelectedGoalId;

    // Temporary storage for real-time session progress
    QMap<int, int> m_currentSessionProgress;

    // Core Functions
    void displayImage(const cv::Mat &frame);
    void detectAndDisplayFaces(cv::Mat &frame);
    bool loadFaceClassifier();
    bool initializeDatabase();
    void logFaceDetection(int faceCount, bool eyesDetected, float focusScore);
    QString calculateStatistics();

    // UI Setup Functions
    void setupUI();
    void createDashboardTab();
    void createWebcamTab();
    void createPomodoroSection(QVBoxLayout *parentLayout);
    void createGoalsSection(QVBoxLayout *parentLayout);
    void createSettingsTab();
    void createAnalyticsTab();
    void createAchievementsTab();
    void updateCharts();
    float calculateHeadPose(const std::vector<cv::Point2f>& landmarks);
    void playAlertSound();
    void exportToCSV();
    void saveSettings();
    void loadSettings();

    // Chart Functions
    void setupCharts();
    void updateChartData();
    void clearCharts();

    void setupDarkAcademiaTheme();

    void createSessionPlanner();
    void implementSessionReview();

    StudyStreak* studyStreak;
    QLabel* streakSummaryLabel;
    StudySession* studySession;

    void showSummaryPopup(bool weekly = false);
};

#endif // MAINWINDOW_H
