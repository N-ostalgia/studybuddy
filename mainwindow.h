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
#include <QGridLayout>
#include <QNetworkAccessManager>
#include <QString>
#include <QCalendarWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// --- WalkthroughOverlay: Top-level class for overlay walkthrough ---
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
class WalkthroughOverlay : public QWidget {
    Q_OBJECT
public:
    struct Step {
        QRect highlightRect;
        QString explanation;
        QString tabName;
    };
    QList<Step> steps;
    int currentStep = 0;
    QWidget* parentWidget = nullptr;
    std::function<void()> onFinish;
    WalkthroughOverlay(QWidget* parent = nullptr) : QWidget(parent), parentWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setFocusPolicy(Qt::StrongFocus);
    }
    void setSteps(const QList<Step>& s) { steps = s; currentStep = 0; update(); emit stepChanged(currentStep); }
    void next() { if (currentStep < steps.size() - 1) { ++currentStep; update(); emit stepChanged(currentStep); } }
    void prev() { if (currentStep > 0) { --currentStep; update(); emit stepChanged(currentStep); } }
    void finish() { if (onFinish) onFinish(); hide(); }
signals:
    void stepChanged(int step);
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(0,0,0,160));
        if (steps.isEmpty()) return;
        QRect hi = steps[currentStep].highlightRect;
        if (!hi.isNull()) {
            QPainterPath path;
            path.addRect(rect());
            path.addRoundedRect(hi, 12, 12);
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillPath(path, Qt::transparent);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            QPen pen(Qt::yellow, 3);
            p.setPen(pen);
            p.drawRoundedRect(hi, 12, 12);
        }
        // Draw explanation box
        QRectF box = QRectF(hi.bottomLeft() + QPoint(0, 16), QSizeF(320, 100));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255,255,255,230));
        p.drawRoundedRect(box, 10, 10);
        p.setPen(Qt::black);
        QFont f = font(); f.setPointSize(11); p.setFont(f);
        p.drawText(box.adjusted(12,8,-12,-8), Qt::TextWordWrap, steps[currentStep].explanation);
        // Draw navigation buttons
        int btnY = box.bottom() + 12;
        int btnW = 80, btnH = 32, gap = 12;
        QRect prevBtn(box.left(), btnY, btnW, btnH);
        QRect nextBtn(box.left() + btnW + gap, btnY, btnW, btnH);
        QRect finishBtn(box.left() + 2*(btnW+gap), btnY, btnW, btnH);
        p.setBrush(QColor(200,200,200,240));
        p.setPen(Qt::gray);
        if (currentStep > 0) p.drawRoundedRect(prevBtn, 8, 8);
        if (currentStep < steps.size()-1) p.drawRoundedRect(nextBtn, 8, 8);
        p.drawRoundedRect(finishBtn, 8, 8);
        p.setPen(Qt::black);
        if (currentStep > 0) p.drawText(prevBtn, Qt::AlignCenter, "Back");
        if (currentStep < steps.size()-1) p.drawText(nextBtn, Qt::AlignCenter, "Next");
        p.drawText(finishBtn, Qt::AlignCenter, "Finish");
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (steps.isEmpty()) return;
        QRectF hi = steps[currentStep].highlightRect;
        QRectF box = QRectF(hi.bottomLeft() + QPoint(0, 16), QSizeF(320, 100));
        int btnY = box.bottom() + 12;
        int btnW = 80, btnH = 32, gap = 12;
        QRect prevBtn(box.left(), btnY, btnW, btnH);
        QRect nextBtn(box.left() + btnW + gap, btnY, btnW, btnH);
        QRect finishBtn(box.left() + 2*(btnW+gap), btnY, btnW, btnH);
        if (currentStep > 0 && prevBtn.contains(e->pos())) { prev(); return; }
        if (currentStep < steps.size()-1 && nextBtn.contains(e->pos())) { next(); return; }
        if (finishBtn.contains(e->pos())) { finish(); return; }
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Right || e->key() == Qt::Key_Down) next();
        else if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Up) prev();
        else if (e->key() == Qt::Key_Escape) finish();
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
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
    //void handleDeleteGoal();
    //void handleStartGoalTracking();
    //void handleStopGoalTracking();
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

    void updateGoalProgressBars();

public slots:
    void showYearlyActivityDialog();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

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
   // QListWidget *goalsListWidget;
    QLineEdit *goalSubjectInput;
    QSpinBox *goalTargetMinutesInput;
    QTextEdit *goalNotesInput;
    QPushButton *addGoalButton;
    //QPushButton *deleteGoalButton;
   // QPushButton *startTrackingButton;
   // QPushButton *stopTrackingButton;
    StudyGoal *studyGoal;
    QComboBox *goalCategoryCombo;
    QPushButton *cancelEditButton;
    QComboBox *recurrenceTypeCombo;
    QLineEdit *recurrenceValueInput;
    QGridLayout *goalsCardLayout; // For the card/grid view of goals
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
    QList<QPushButton*> surveyStarButtons;
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
    void createSurveysTab();
    void createCalendarTab();
    void createHelpTab();

    // Chart Functions
    void setupCharts();
    void updateChartData();
    void clearCharts();

    void setupDarkAcademiaTheme();



    StudyStreak* studyStreak;
    QLabel* streakSummaryLabel;
    StudySession* studySession;

    void showSummaryPopup(bool weekly = false);

    QTimer *progressUpdateTimer;

    QProgressBar *m_activeGoalProgressBar = nullptr;
    QGroupBox *createGoalGroup;
    void openResourceInApp(const QString &res);

    // Buddy Chat UI
    QListWidget* buddyChatList = nullptr;
    QLineEdit* buddyChatInput = nullptr;
    QPushButton* buddyChatSendButton = nullptr;
    QPushButton* buddyAttachButton = nullptr;
    QPushButton* buddySpeakerButton = nullptr;
    QListWidget* buddyConversationList = nullptr;
    QPushButton* buddyNewConversationButton = nullptr;
    QPushButton* buddyDeleteConversationButton = nullptr;
    QLineEdit* buddyConversationSearch = nullptr;
    QString buddyConversationSearchText;
    QList<QPushButton*> buddyPromptButtons;
    QString buddyPendingImagePath;
    struct Conversation {
        QString id;
        QString title;
        QDateTime timestamp;
        QList<QPair<QString, QString>> messages; // role, message
    };
    QList<Conversation> buddyConversations;
    int currentConversationIndex = -1;
    void createBuddyChatTab();
    void handleBuddyChatSend();
    void handleBuddyAttach();
    void handleBuddySpeaker(const QString& text);
    void loadBuddyConversations();
    void saveBuddyConversations();
    void startNewBuddyConversation();
    void switchBuddyConversation(int index);
    void deleteBuddyConversation(int index);
    void filterBuddyConversations(const QString& text);
    void handleBuddyPromptClicked();

    // Gemini API integration
    QNetworkAccessManager* geminiNetworkManager = nullptr;
    QString GEMINI_API_KEY = "AIzaSyBYkxeqX0en7k7aF4Huk1-5YBgIqdnBQBA";

    // Notification settings checkboxes
    QCheckBox *enableDesktopNotif = nullptr;
    QCheckBox *notifPomodoro = nullptr;
    QCheckBox *notifAchievement = nullptr;
    QCheckBox *notifStreak = nullptr;
    QCheckBox *notifSessionReminder = nullptr;

    // Pomodoro and streak settings spinboxes
    QSpinBox *studyDurationSpin = nullptr;
    QSpinBox *shortBreakSpin = nullptr;
    QSpinBox *longBreakSpin = nullptr;
    QSpinBox *cyclesSpin = nullptr;
    QSpinBox *minStudySpin = nullptr;

    void notifyOverdueAndTodayGoals();

    QCalendarWidget *calendarWidget = nullptr;
    QListWidget *calendarGoalsList = nullptr;
    void openDayPlannerDialog(const QDate& date);

    QMap<QString, int> loadFreeTimeForDate(const QDate& date);
    void saveFreeTimeForDate(const QDate& date, int morning, int evening, int night);

    // Walkthrough overlay widget
    QPointer<WalkthroughOverlay> walkthroughOverlay;
    void startWalkthroughOverlay();
};

#endif // MAINWINDOW_H
