#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "analytics.h"
#include "achievements.h"
#include "survey.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <cmath>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QTabWidget>
#include <QToolBar>
#include <QGroupBox>
#include <QProgressBar>
#include <QFileDialog>
#include <QMenu>
#include <QtMultimedia/QSoundEffect>
#include <QUrl>
#include <QTextStream>
#include <QTime>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QChartView>
#include <QPainter>
#include <QDateEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QSizePolicy>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QTimer>
#include <QDate>
#include <QFrame>
#include <QSlider>
#include <QCheckBox>
#include <QWebEngineView>
#include <QPdfView>
#include <QPdfDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSplitter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QInputDialog>
#include <QBuffer>
#include <QImageReader>
#ifdef Q_OS_WIN
#include <QAxObject>
#endif
#include <QSettings>
#include <QTabBar>

bool MainWindow::initializeDatabase()
{
    // Use a single connection name for the unified database
    const QString connName = "StudyGoalsConnection";
    if (QSqlDatabase::contains(connName)) {
        db = QSqlDatabase::database(connName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connName);
        QString dbPath = QCoreApplication::applicationDirPath() + "/studybuddy.db";
    db.setDatabaseName(dbPath);
    }

    qDebug() << "Attempting to open database at:" << db.databaseName();

    if (!db.open()) {
        qDebug() << "Error: Unable to open database" << db.lastError().text();
        QMessageBox::critical(this, "Database Error", "Unable to open the database: " + db.lastError().text());
        return false;
    }

    qDebug() << "Database connected successfully at" << db.databaseName();

    QSqlQuery query(db);
    // Create all tables for the unified schema
    QStringList createTableQueries = {
        // detections
        "CREATE TABLE IF NOT EXISTS detections (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp TEXT NOT NULL, face_count INTEGER NOT NULL, eyes_detected BOOLEAN NOT NULL, blink_count INTEGER NOT NULL, focus_score REAL NOT NULL)",
        // session_plans
        "CREATE TABLE IF NOT EXISTS session_plans (id INTEGER PRIMARY KEY AUTOINCREMENT, subject TEXT NOT NULL, goals TEXT, resource_link TEXT, mental_state TEXT, plan_date TEXT NOT NULL)",
        // session_reviews
        "CREATE TABLE IF NOT EXISTS session_reviews (id INTEGER PRIMARY KEY AUTOINCREMENT, session_plan_id INTEGER NOT NULL, focus_score REAL, distraction_events TEXT, effectiveness TEXT, notes TEXT, review_date TEXT NOT NULL, session_id INTEGER, FOREIGN KEY(session_plan_id) REFERENCES session_plans(id))",
        // study_sessions
        "CREATE TABLE IF NOT EXISTS study_sessions (id INTEGER PRIMARY KEY AUTOINCREMENT, planned_session_id INTEGER, start_time TEXT, end_time TEXT, type TEXT, notes TEXT)",
        // goals
        "CREATE TABLE IF NOT EXISTS goals (id INTEGER PRIMARY KEY AUTOINCREMENT, subject TEXT NOT NULL, target_minutes INTEGER NOT NULL, completed_minutes INTEGER DEFAULT 0, start_date TEXT NOT NULL, due_date TEXT, status TEXT DEFAULT 'Active', notes TEXT, recurrence_type TEXT DEFAULT 'None', recurrence_value TEXT, last_generated_date TEXT, category TEXT DEFAULT 'Uncategorized')",
        // goal_resources
        "CREATE TABLE IF NOT EXISTS goal_resources (id INTEGER PRIMARY KEY AUTOINCREMENT, goal_id INTEGER NOT NULL, type TEXT NOT NULL, value TEXT NOT NULL, description TEXT, FOREIGN KEY(goal_id) REFERENCES goals(id))",
        // session_resources
        "CREATE TABLE IF NOT EXISTS session_resources (id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, type TEXT NOT NULL, value TEXT NOT NULL, description TEXT, FOREIGN KEY(session_id) REFERENCES study_sessions(id))",
        // session_goals
        "CREATE TABLE IF NOT EXISTS session_goals (id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, goal_id INTEGER NOT NULL, FOREIGN KEY(session_id) REFERENCES study_sessions(id), FOREIGN KEY(goal_id) REFERENCES goals(id))",
        // surveys
        "CREATE TABLE IF NOT EXISTS surveys (id INTEGER PRIMARY KEY AUTOINCREMENT, goal_id INTEGER NOT NULL, timestamp TEXT NOT NULL, mood_emoji TEXT, distraction_level INTEGER, distractions TEXT, session_satisfaction INTEGER, goal_achieved TEXT, open_feedback TEXT, set_reminder INTEGER)",
        // study_streaks
        "CREATE TABLE IF NOT EXISTS study_streaks (id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL UNIQUE, study_minutes INTEGER NOT NULL, streak_count INTEGER NOT NULL, created_at TEXT NOT NULL)",
        // achievements
        "CREATE TABLE IF NOT EXISTS achievements (id TEXT PRIMARY KEY, name TEXT NOT NULL, description TEXT NOT NULL, category TEXT NOT NULL, required_value INTEGER NOT NULL, icon_path TEXT, unlocked_at TEXT, progress INTEGER DEFAULT 0)",
        // free_time
        "CREATE TABLE IF NOT EXISTS free_time (date TEXT PRIMARY KEY, morning_minutes INTEGER, evening_minutes INTEGER, night_minutes INTEGER)"
    };
    for (const QString& q : createTableQueries) {
        if (!query.exec(q)) {
        qDebug() << "Error: Failed to create table" << query.lastError().text();
            qDebug() << "Query was:" << q;
        QMessageBox::critical(this, "Database Error", "Failed to create table: " + query.lastError().text());
        return false;
    }
    }
    qDebug() << "All tables for studybuddy.db initialized successfully";
    return true;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , webcamActive(false)
    , faceDetectionEnabled(true)
    , alertsEnabled(true)
    , alertThreshold(50)
    , eyesPreviouslyDetected(false)
    , blinkCount(0)
    , blinkRate(0.0)
    , consecutiveNoEyeFrames(0)
    , totalFrames(0)
    , focusedFrames(0)
    , currentFocusScore(0.0)
    , m_currentActiveGoalId(-1)
    , m_currentSelectedGoalId(-1)
    , studySession(nullptr)
{
    ui->setupUi(this);
    setupCharts();
    geminiNetworkManager = new QNetworkAccessManager(this);
    loadApiKey();
    if (!initializeDatabase()) {
        QMessageBox::critical(this, "Error", "Failed to initialize the database.");
        return;
    }

    // Create StudySession after database is initialized
    studySession = new StudySession(db, this);
    if (!studySession) {
        QMessageBox::critical(this, "Error", "Failed to create StudySession.");
        return;
    }

    // Create StudyGoal and StudyStreak objects
    studyGoal = new StudyGoal(db, this);
    studyStreak = new StudyStreak(db, this);
    pomodoroTimer = new PomodoroTimer(this);
    achievementTracker = new Achievement(db, this);
    alertSound = new QSoundEffect(this);

    // Ensure recurring goals are generated for today
    studyGoal->checkAndGenerateAllRecurringGoals();

    if (!studyStreak->initializeStreaks()) {
        QMessageBox::critical(this, "Error", "Failed to initialize StudyStreak.");
        return;
    }
    if (!achievementTracker->initializeAchievements()) {
        QMessageBox::critical(this, "Error", "Failed to initialize Achievement tracker.");
        return;
    }

    setupUI();
    setupDarkAcademiaTheme();
    loadSettings();
    
    if (streakSummaryLabel) {
        streakSummaryLabel->setText(QString("Current Streak: %1 days").arg(studyStreak->getCurrentStreak()));
    }
    if (!loadFaceClassifier()) {
        QMessageBox::warning(this, "Warning", "Failed to load face classifier. Face detection will be disabled.");
        faceDetectionEnabled = false;
    }

    showSummaryPopup(false);

    // system tray icon
    trayIcon = new QSystemTrayIcon(QIcon(":/icons/app_icon.png"), this);
    QMenu *trayMenu = new QMenu(this);
    trayMenu->addAction("Show", this, &QWidget::show);
    trayMenu->addAction("Exit", qApp, &QApplication::quit);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    // timer for periodic updates
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateDashboard);

    // alert sound
    alertSound->setSource(QUrl("qrc:/sounds/alert.wav"));
    alertSound->setVolume(0.75f);

    // Connect Pomodoro signals to slots
    connect(pomodoroTimer, &PomodoroTimer::timeUpdated, this, &MainWindow::updatePomodoroDisplay);
    connect(pomodoroTimer, &PomodoroTimer::modeChanged, this, &MainWindow::handlePomodoroModeChanged);
    connect(pomodoroTimer, &PomodoroTimer::cycleChanged, this, &MainWindow::handlePomodoroCycleChanged);
    connect(pomodoroTimer, &PomodoroTimer::timerCompleted, this, &MainWindow::handlePomodoroTimerCompleted);

    // Connect StudyGoal to PomodoroTimer
    studyGoal->connectToPomodoroTimer(pomodoroTimer);

    // Connect StudyGoal signals to slots
    connect(studyGoal, &StudyGoal::goalCreated, this, &MainWindow::handleGoalCreated);
    connect(studyGoal, &StudyGoal::goalUpdated, this, &MainWindow::handleGoalUpdated);
    connect(studyGoal, &StudyGoal::goalDeleted, this, &MainWindow::handleGoalDeleted);
    connect(studyGoal, &StudyGoal::sessionTimeUpdated, this, &MainWindow::onGoalSessionTimeUpdated);
    connect(studyGoal, &StudyGoal::sessionProgressUpdated, this, &MainWindow::onGoalSessionProgressUpdated);
    connect(studyGoal, &StudyGoal::goalTrackingStarted, this, &MainWindow::onGoalTrackingStarted);
    connect(studyGoal, &StudyGoal::goalTrackingStopped, this, &MainWindow::onGoalTrackingStopped);

    // Connect StudyStreak signals to slots
    connect(studyStreak, &StudyStreak::streakUpdated, this, [this](int currentStreak) {
        if (streakSummaryLabel) streakSummaryLabel->setText(QString("Current Streak: %1 days").arg(currentStreak));
    });
    connect(studyStreak, &StudyStreak::milestoneReached, this, [this](int days) {
        QMessageBox::information(this, "Streak Milestone!", QString("Congratulations! You've reached a %1-day study streak!").arg(days));
    });

    setWindowTitle("Study Buddy - Focus Monitor");
    resize(800, 600);

    // Add toolbar actions for summary popups
    QToolBar *toolbar = findChild<QToolBar *>();
    if (toolbar) {
        toolbar->addAction("Show Daily Summary", this, [this]() { showSummaryPopup(false); });
        toolbar->addAction("Show Weekly Summary", this, [this]() { showSummaryPopup(true); });
        toolbar->addAction("Export All", this, &MainWindow::exportToCSV);
        QAction* settingsAction = toolbar->addAction("Settings");
        connect(settingsAction, &QAction::triggered, this, [this]() {
            // Switch to the Settings tab (index may change if tabs are reordered)
            for (int i = 0; i < tabWidget->count(); ++i) {
                if (tabWidget->tabText(i) == "Settings") {
                    tabWidget->setCurrentIndex(i);
                    break;
                }
            }
        });
    }

    progressUpdateTimer = new QTimer(this);
    progressUpdateTimer->setInterval(1000); // 1 second
    connect(progressUpdateTimer, &QTimer::timeout, this, &MainWindow::updateGoalProgressBars);

    // At the end of MainWindow constructor, after UI setup:
    notifyOverdueAndTodayGoals();

    // After daily summary and after UI is shown, show overlay walkthrough if not shown before
    QSettings settings("StudyBuddy", "FocusMonitor");
    bool walkthroughShown = settings.value("walkthroughShown", false).toBool();
    if (!walkthroughShown) {
        QTimer::singleShot(500, this, [this]() {
            startWalkthroughOverlay();
            QSettings settings("StudyBuddy", "FocusMonitor");
            settings.setValue("walkthroughShown", true);
        });
    }
}

void MainWindow::setupUI()
{
    // Create main tab widget
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // Create and add tabs
    createDashboardTab();
    createWebcamTab();
    createBuddyChatTab();
    createAchievementsTab();
    createSurveysTab();
    createAnalyticsTab();
    createCalendarTab();
    createSettingsTab();
    createHelpTab();

    // Create toolbar
    QToolBar *toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("Start", this, &MainWindow::startWebcam);
    toolbar->addAction("Stop", this, &MainWindow::stopWebcam);
    toolbar->addAction("History", this, &MainWindow::showDetectionHistory);
    toolbar->addAction("Export", this, &MainWindow::exportData);
    toolbar->addAction("Yearly Activity", this, &MainWindow::showYearlyActivityDialog);
}

void MainWindow::createWebcamTab()
{
    QWidget *webcamTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(webcamTab);

    // Webcam display
    webcamLabel = new QLabel();
    webcamLabel->setMinimumSize(640, 480);
    webcamLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(webcamLabel);

    // Controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    startButton = new QPushButton("Start Session");
    stopButton = new QPushButton("Stop Session");
    enableFaceDetectionCheckbox = new QCheckBox("Enable Face Detection");
    enableFaceDetectionCheckbox->setChecked(true);
    controlsLayout->addWidget(startButton);
    controlsLayout->addWidget(stopButton);
    controlsLayout->addWidget(enableFaceDetectionCheckbox);
    
    layout->addLayout(controlsLayout);

    // Status indicators
    QGroupBox *statusGroup = new QGroupBox("Session Status");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    
    blinkCountLabel = new QLabel("Blinks: 0");
    focusScoreLabel = new QLabel("Focus Score: 0%");
    sessionDurationLabel = new QLabel("Duration: 00:00:00");
    
    statusLayout->addWidget(blinkCountLabel);
    statusLayout->addWidget(focusScoreLabel);
    statusLayout->addWidget(sessionDurationLabel);
    
    layout->addWidget(statusGroup);

    // Connect signals
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startWebcam);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopWebcam);
    connect(enableFaceDetectionCheckbox, &QCheckBox::toggled, [this](bool checked) {
        faceDetectionEnabled = checked;
    });

    tabWidget->addTab(webcamTab, "Webcam");
}

void MainWindow::createDashboardTab()
{
    QWidget *dashboardTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(dashboardTab);

    // Create a scroll area
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    // Create a widget to hold the dashboard content
    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    // Add Pomodoro and Study Goals sections at the top
    createPomodoroSection(scrollLayout);
    createGoalsSection(scrollLayout);

    // Focus progress
    QGroupBox *focusGroup = new QGroupBox("Focus Metrics");
    QVBoxLayout *focusLayout = new QVBoxLayout(focusGroup);
    
    focusProgressBar = new QProgressBar();
    focusProgressBar->setRange(0, 100);
    focusProgressBar->setValue(0);
    
    focusLayout->addWidget(new QLabel("Current Focus Level:"));
    focusLayout->addWidget(focusProgressBar);
    
    scrollLayout->addWidget(focusGroup);

    // History controls
    QHBoxLayout *historyControls = new QHBoxLayout();
    showHistoryButton = new QPushButton("Show Detailed History");
    exportButton = new QPushButton("Export Data");
    
    historyControls->addWidget(showHistoryButton);
    historyControls->addWidget(exportButton);
    
    scrollLayout->addLayout(historyControls);

    scrollArea->setWidget(scrollWidget);

    layout->addWidget(scrollArea);

    // Connect signals
    connect(showHistoryButton, &QPushButton::clicked, this, &MainWindow::showDetectionHistory);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportData);

    // Streak summary label
    QLabel* streakSummaryLabel = new QLabel("Current Streak: 0 days");
    streakSummaryLabel->setObjectName("streakSummaryLabel");
    layout->addWidget(streakSummaryLabel);
    this->streakSummaryLabel = streakSummaryLabel;

    tabWidget->addTab(dashboardTab, "Dashboard");
}

void MainWindow::createPomodoroSection(QVBoxLayout *parentLayout)
{
    QGroupBox *pomodoroGroup = new QGroupBox("Pomodoro Timer");
    QVBoxLayout *pomodoroLayout = new QVBoxLayout(pomodoroGroup);

    // Timer display
    pomodoroTimeLabel = new QLabel("25:00");
    pomodoroTimeLabel->setAlignment(Qt::AlignCenter);
    QFont font = pomodoroTimeLabel->font();
    font.setPointSize(48);
    pomodoroTimeLabel->setFont(font);
    pomodoroLayout->addWidget(pomodoroTimeLabel);

    // Mode and Cycle display
    QHBoxLayout *infoLayout = new QHBoxLayout();
    pomodoroModeLabel = new QLabel("Mode: Study");
    pomodoroCycleLabel = new QLabel("Cycle: 0");
    infoLayout->addWidget(pomodoroModeLabel);
    infoLayout->addWidget(pomodoroCycleLabel);
    pomodoroLayout->addLayout(infoLayout);

    // Controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    pomodoroStartButton = new QPushButton("Start");
    pomodoroPauseButton = new QPushButton("Pause");
    pomodoroResetButton = new QPushButton("Reset");
    pomodoroSkipButton = new QPushButton("Skip");

    controlsLayout->addWidget(pomodoroStartButton);
    controlsLayout->addWidget(pomodoroPauseButton);
    controlsLayout->addWidget(pomodoroResetButton);
    controlsLayout->addWidget(pomodoroSkipButton);
    pomodoroLayout->addLayout(controlsLayout);

    // Connect controls to PomodoroTimer methods
    connect(pomodoroStartButton, &QPushButton::clicked, pomodoroTimer, &PomodoroTimer::start);
    connect(pomodoroPauseButton, &QPushButton::clicked, pomodoroTimer, &PomodoroTimer::pause);
    connect(pomodoroResetButton, &QPushButton::clicked, pomodoroTimer, &PomodoroTimer::reset);
    connect(pomodoroSkipButton, &QPushButton::clicked, pomodoroTimer, &PomodoroTimer::skip);

    parentLayout->addWidget(pomodoroGroup);
}

void MainWindow::createGoalsSection(QVBoxLayout *parentLayout)
{
    // Goal Creation Section (unchanged)
    createGoalGroup = new QGroupBox("Create New Goal");
    QFormLayout *createGoalLayout = new QFormLayout(createGoalGroup);

    goalSubjectInput = new QLineEdit();
    goalTargetMinutesInput = new QSpinBox();
    goalTargetMinutesInput->setRange(1, 1000);
    goalNotesInput = new QTextEdit();
    
    goalCategoryCombo = new QComboBox();
    goalCategoryCombo->addItems({"Uncategorized", "Academic", "Personal", "Work", "Health", "Other"});

    recurrenceTypeCombo = new QComboBox();
    recurrenceTypeCombo->addItems({"None", "Daily", "Weekly", "Monthly"});
    recurrenceValueInput = new QLineEdit();
    recurrenceValueInput->setPlaceholderText("e.g., 'Mon,Wed,Fri' for weekly");
    recurrenceValueInput->setEnabled(false);
    
    connect(recurrenceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                recurrenceValueInput->setEnabled(index > 0);
            });

    // --- Resources input ---
    QHBoxLayout *resourceInputLayout = new QHBoxLayout();
    QLineEdit *resourceInput = new QLineEdit();
    resourceInput->setPlaceholderText("Paste YouTube link or file path...");
    QPushButton *browseResourceBtn = new QPushButton("Browse");
    resourceInputLayout->addWidget(resourceInput);
    resourceInputLayout->addWidget(browseResourceBtn);
    QListWidget *resourceList = new QListWidget();
    resourceList->setMinimumHeight(40);
    QPushButton *addResourceBtn = new QPushButton("Add Resource");
    QVBoxLayout *resourceSection = new QVBoxLayout();
    resourceSection->addLayout(resourceInputLayout);
    resourceSection->addWidget(addResourceBtn);
    resourceSection->addWidget(resourceList);
    createGoalLayout->addRow("Resources:", resourceSection);
    // Store resources in a QStringList
    QStringList *resources = new QStringList();
    connect(browseResourceBtn, &QPushButton::clicked, this, [resourceInput, this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Resource", QString(), "All Files (*.*)");
        if (!file.isEmpty()) resourceInput->setText(file);
    });
    connect(addResourceBtn, &QPushButton::clicked, this, [resourceInput, resourceList, resources]() {
        QString res = resourceInput->text().trimmed();
        if (!res.isEmpty() && !resources->contains(res)) {
            resources->append(res);
            resourceList->addItem(res);
            resourceInput->clear();
        }
    });
    connect(resourceList, &QListWidget::itemDoubleClicked, this, [resourceList, resources](QListWidgetItem *item) {
        resources->removeAll(item->text());
        delete item;
            });
    
    addGoalButton = new QPushButton("Add Goal");
    cancelEditButton = new QPushButton("Cancel Edit");
    cancelEditButton->setEnabled(false);

    createGoalLayout->addRow("Subject:", goalSubjectInput);
    createGoalLayout->addRow("Target Minutes:", goalTargetMinutesInput);
    createGoalLayout->addRow("Notes:", goalNotesInput);
    createGoalLayout->addRow("Category:", goalCategoryCombo);
    createGoalLayout->addRow("Recurrence:", recurrenceTypeCombo);
    createGoalLayout->addRow("Recurrence Details:", recurrenceValueInput);
    QHBoxLayout *addEditButtonsLayout = new QHBoxLayout();
    addEditButtonsLayout->addWidget(addGoalButton);
    addEditButtonsLayout->addWidget(cancelEditButton);
    createGoalLayout->addRow(addEditButtonsLayout);
    parentLayout->addWidget(createGoalGroup);

    // --- Current Goals Section (Card/List View, Grid) ---
    QGroupBox *currentGoalsGroup = new QGroupBox("Current Goals");
    QVBoxLayout *currentGoalsLayout = new QVBoxLayout(currentGoalsGroup);

    QScrollArea *goalsScrollArea = new QScrollArea();
    goalsScrollArea->setWidgetResizable(true);
    goalsScrollArea->setMinimumHeight(400); 
    goalsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QWidget *goalsContainer = new QWidget();
    goalsCardLayout = new QGridLayout(goalsContainer);
    goalsCardLayout->setSpacing(16);
    goalsCardLayout->setContentsMargins(8, 8, 8, 8);

    goalsScrollArea->setWidget(goalsContainer);
    currentGoalsLayout->addWidget(goalsScrollArea);

    parentLayout->addWidget(currentGoalsGroup);

   
    // --- Goal Add/Update Logic ---
    connect(addGoalButton, &QPushButton::clicked, this, [this, resources]() {
        QString subject = goalSubjectInput->text();
        int targetMinutes = goalTargetMinutesInput->value();
        QString notes = goalNotesInput->toPlainText();
        QString recurrenceType = recurrenceTypeCombo->currentText();
        QString recurrenceValue = recurrenceValueInput->text();
        QString category = goalCategoryCombo->currentText();
        QStringList resourceList = *resources;
        if (subject.isEmpty() || targetMinutes <= 0) {
            QMessageBox::warning(this, "Input Error", "Subject and Target Minutes cannot be empty.");
            return;
        }
        if (m_currentSelectedGoalId != -1) {
            if (studyGoal->updateGoal(m_currentSelectedGoalId, subject, targetMinutes, notes, recurrenceType, recurrenceValue, category, resourceList)) {
                QMessageBox::information(this, "Success", "Goal updated successfully!");
                handleCancelEdit();
            } else {
                QMessageBox::critical(this, "Error", "Failed to update goal.");
            }
        } else {
            if (studyGoal->createGoal(subject, targetMinutes, notes, recurrenceType, recurrenceValue, category, resourceList)) {
                QMessageBox::information(this, "Success", "Goal created successfully!");
                refreshGoalsDisplay();
            } else {
                QMessageBox::critical(this, "Error", "Failed to create goal.");
            }
        }
    });
    
    connect(cancelEditButton, &QPushButton::clicked, this, &MainWindow::handleCancelEdit);

    refreshGoalsDisplay();
}

void MainWindow::createAnalyticsTab()
{
    QWidget *analyticsTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(analyticsTab);
    layout->setContentsMargins(0,0,0,0);

    // Analytics Section
    QGroupBox *analyticsGroup = new QGroupBox("Study Analytics");
    analyticsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout *analyticsLayout = new QVBoxLayout(analyticsGroup);
    analyticsLayout->setContentsMargins(0,0,0,0);
    StudyAnalytics *analytics = new StudyAnalytics(this->db, this);
    
    // Add date range selection
    QHBoxLayout *dateRangeLayout = new QHBoxLayout();
    QDateEdit *startDateEdit = new QDateEdit();
    QDateEdit *endDateEdit = new QDateEdit();
    startDateEdit->setDate(QDate::currentDate().addDays(-7));
    endDateEdit->setDate(QDate::currentDate());
    dateRangeLayout->addWidget(new QLabel("From:"));
    dateRangeLayout->addWidget(startDateEdit);
    dateRangeLayout->addWidget(new QLabel("To:"));
    dateRangeLayout->addWidget(endDateEdit);
    analyticsLayout->addLayout(dateRangeLayout);

    QVBoxLayout *chartsLayout = new QVBoxLayout();
    QChartView *focusChartView = new QChartView(analytics->createFocusChart(startDateEdit->date(), endDateEdit->date()));
    focusChartView->setMinimumSize(500, 250);
    focusChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    focusChartView->setToolTip("Shows your average focus score per day.");
    QChartView *productivityChartView = new QChartView(analytics->createProductivityChart(startDateEdit->date(), endDateEdit->date()));
    productivityChartView->setMinimumSize(500, 250);
    productivityChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    productivityChartView->setToolTip("Shows the number of study detections (activity) per day.");
    QChartView *goalChartView = new QChartView(analytics->createGoalCompletionChart(startDateEdit->date(), endDateEdit->date()));
    goalChartView->setMinimumSize(500, 250);
    goalChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    goalChartView->setToolTip("Shows the percentage of goals completed vs. incomplete.");
    QChartView *subjectTimeChartView = new QChartView(analytics->createSubjectTimeChart(startDateEdit->date(), endDateEdit->date()));
    subjectTimeChartView->setMinimumSize(500, 250);
    subjectTimeChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    subjectTimeChartView->setToolTip("Shows how much time you spent on each subject.");
    QChartView *distractionBarChartView = new QChartView(analytics->createDistractionBarChart(startDateEdit->date(), endDateEdit->date()));
    distractionBarChartView->setMinimumSize(500, 250);
    distractionBarChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    distractionBarChartView->setToolTip("Shows which distractions you reported most often in surveys.");
    chartsLayout->addWidget(focusChartView);
    chartsLayout->addWidget(productivityChartView);
    chartsLayout->addWidget(goalChartView);
    chartsLayout->addWidget(subjectTimeChartView);
    chartsLayout->addWidget(distractionBarChartView);
    analyticsLayout->addLayout(chartsLayout);
    analyticsTab->setToolTip("Explore your study patterns, focus, and progress here.");

    // Add insights section
    QTextEdit *insightsText = new QTextEdit();
    insightsText->setReadOnly(true);
    analyticsLayout->addWidget(new QLabel("Study Insights:"));
    analyticsLayout->addWidget(insightsText);

    // Connect signals
    connect(analytics, &StudyAnalytics::patternDetected, [insightsText](const QString &pattern) {
        insightsText->append("Pattern: " + pattern);
    });

    connect(analytics, &StudyAnalytics::productivityInsight, [insightsText](const QString &insight) {
        insightsText->append("Insight: " + insight);
    });

    analytics->analyzePatterns(startDateEdit->date(), endDateEdit->date());

    auto updateAllCharts = [=]() {
        focusChartView->setChart(analytics->createFocusChart(startDateEdit->date(), endDateEdit->date()));
        productivityChartView->setChart(analytics->createProductivityChart(startDateEdit->date(), endDateEdit->date()));
        goalChartView->setChart(analytics->createGoalCompletionChart(startDateEdit->date(), endDateEdit->date()));
        subjectTimeChartView->setChart(analytics->createSubjectTimeChart(startDateEdit->date(), endDateEdit->date()));
        distractionBarChartView->setChart(analytics->createDistractionBarChart(startDateEdit->date(), endDateEdit->date()));
        insightsText->clear();
        analytics->analyzePatterns(startDateEdit->date(), endDateEdit->date());
    };

    connect(startDateEdit, &QDateEdit::dateChanged, updateAllCharts);
    connect(endDateEdit, &QDateEdit::dateChanged, updateAllCharts);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(analyticsGroup);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(scrollArea);
    tabWidget->addTab(analyticsTab, "Analytics");
}


void MainWindow::createSettingsTab()
{
    QWidget *settingsTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(settingsTab);

    // Alert settings
    QGroupBox *alertGroup = new QGroupBox("Alert Settings");
    QVBoxLayout *alertLayout = new QVBoxLayout(alertGroup);
    QCheckBox *enableAlertsCheckbox = new QCheckBox("Enable Focus Alerts");
    enableAlertsCheckbox->setChecked(alertsEnabled);
    QSpinBox *alertThresholdSpin = new QSpinBox();
    alertThresholdSpin->setRange(1, 60);
    alertThresholdSpin->setValue(alertThreshold);
    alertLayout->addWidget(enableAlertsCheckbox);
    alertLayout->addWidget(new QLabel("Alert Threshold (seconds):"));
    alertLayout->addWidget(alertThresholdSpin);
    layout->addWidget(alertGroup);
    connect(enableAlertsCheckbox, &QCheckBox::toggled, [this](bool checked) {
        alertsEnabled = checked;
        saveSettings();
    });
    connect(alertThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){
        alertThreshold = v;
        saveSettings();
    });

    // Pomodoro settings
    QGroupBox *pomodoroGroup = new QGroupBox("Pomodoro Timer Settings");
    QFormLayout *pomodoroLayout = new QFormLayout(pomodoroGroup);
    QSpinBox *studyDurationSpin = new QSpinBox();
    studyDurationSpin->setRange(10, 120);
    shortBreakSpin = new QSpinBox();
    shortBreakSpin->setRange(1, 30);
    longBreakSpin = new QSpinBox();
    longBreakSpin->setRange(5, 60);
    cyclesSpin = new QSpinBox();
    cyclesSpin->setRange(1, 10);
    minStudySpin = new QSpinBox();
    minStudySpin->setRange(1, 180);
    pomodoroLayout->addRow("Study Duration (min):", studyDurationSpin);
    pomodoroLayout->addRow("Short Break (min):", shortBreakSpin);
    pomodoroLayout->addRow("Long Break (min):", longBreakSpin);
    pomodoroLayout->addRow("Cycles Before Long Break:", cyclesSpin);
    layout->addWidget(pomodoroGroup);
    connect(studyDurationSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){ pomodoroTimer->setStudyDuration(v); saveSettings(); });
    connect(shortBreakSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){ pomodoroTimer->setBreakDuration(v); saveSettings(); });
    connect(longBreakSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){ pomodoroTimer->setLongBreakDuration(v); saveSettings(); });
    connect(cyclesSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){ pomodoroTimer->setCyclesBeforeLongBreak(v); saveSettings(); });

    // Minimum minutes for study day
    QGroupBox *streakGroup = new QGroupBox("Streak/Yearly Activity");
    QFormLayout *streakLayout = new QFormLayout(streakGroup);
    QSpinBox *minStudySpin = new QSpinBox();
    minStudySpin->setRange(1, 180); minStudySpin->setValue(1);
    streakLayout->addRow("Minimum minutes to count as study day:", minStudySpin);
    layout->addWidget(streakGroup);
    connect(minStudySpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){
        // You would use this value in your streak/yearly activity logic
        saveSettings();
    });

    layout->addStretch();
    tabWidget->addTab(settingsTab, "Settings");

    // Notification settings
    QGroupBox *notifGroup = new QGroupBox("Notification Settings");
    QVBoxLayout *notifLayout = new QVBoxLayout(notifGroup);
    enableDesktopNotif = new QCheckBox("Enable desktop notifications");
    notifPomodoro = new QCheckBox("Notify on Pomodoro completion");
    notifAchievement = new QCheckBox("Notify on achievement unlock");
    notifStreak = new QCheckBox("Notify on streak milestone");
    notifSessionReminder = new QCheckBox("Notify before session starts");
    enableDesktopNotif->setChecked(true);
    notifPomodoro->setChecked(true);
    notifAchievement->setChecked(true);
    notifStreak->setChecked(true);
    notifSessionReminder->setChecked(false);
    notifLayout->addWidget(enableDesktopNotif);
    notifLayout->addWidget(notifPomodoro);
    notifLayout->addWidget(notifAchievement);
    notifLayout->addWidget(notifStreak);
    notifLayout->addWidget(notifSessionReminder);
    layout->addWidget(notifGroup);
    connect(enableDesktopNotif, &QCheckBox::toggled, [this](bool){ saveSettings(); });
    connect(notifPomodoro, &QCheckBox::toggled, [this](bool){ saveSettings(); });
    connect(notifAchievement, &QCheckBox::toggled, [this](bool){ saveSettings(); });
    connect(notifStreak, &QCheckBox::toggled, [this](bool){ saveSettings(); });
    connect(notifSessionReminder, &QCheckBox::toggled, [this](bool){ saveSettings(); });

    QSettings settings("StudyBuddy", "FocusMonitor");
    studyDurationSpin->setValue(settings.value("pomodoroStudyDuration", 25).toInt());
    shortBreakSpin->setValue(settings.value("pomodoroShortBreak", 5).toInt());
    longBreakSpin->setValue(settings.value("pomodoroLongBreak", 15).toInt());
    cyclesSpin->setValue(settings.value("pomodoroCycles", 4).toInt());
    minStudySpin->setValue(settings.value("minStudyMinutes", 1).toInt());

    // At the end of createSettingsTab()
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton("Save");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);

    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        QMessageBox::information(this, "Settings", "Settings saved!");
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        loadSettings();
    });
}

void MainWindow::createAchievementsTab()
{
    QWidget *achievementsTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(achievementsTab);

    QGroupBox *achievementsGroup = new QGroupBox("Achievements");
    QVBoxLayout *achievementsLayout = new QVBoxLayout(achievementsGroup);

    QWidget *iconGridWidget = new QWidget();
    QGridLayout *iconGrid = new QGridLayout(iconGridWidget);
    iconGrid->setSpacing(32);
    iconGrid->setAlignment(Qt::AlignCenter);

    // Get all achievements (unlocked + locked)
    QList<Achievement::AchievementInfo> unlocked = achievementTracker->getUnlockedAchievements();
    QList<Achievement::AchievementInfo> locked = achievementTracker->getLockedAchievements();
    QList<Achievement::AchievementInfo> allAchievements = unlocked + locked;
    int columns = 4;
    int row = 0, col = 0;
    for (const auto &info : allAchievements) {
        QPushButton *iconButton = new QPushButton();
        QIcon icon(info.iconPath);
        bool isUnlocked = achievementTracker->isAchievementUnlocked(info.id);
        if (!isUnlocked) {
            // Fade locked achievements
            QPixmap pixmap = icon.pixmap(96, 96);
            QPixmap faded(pixmap.size());
            faded.fill(Qt::transparent);
            QPainter p(&faded);
            p.setOpacity(0.3);
            p.drawPixmap(0, 0, pixmap);
            p.end();
            iconButton->setIcon(QIcon(faded));
        } else {
            iconButton->setIcon(icon);
        }
        iconButton->setIconSize(QSize(96, 96));
        iconButton->setFixedSize(110, 130);
        iconButton->setFlat(true);
        iconButton->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:focus { outline: none; }");
        QLabel *titleLabel = new QLabel(info.name);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-weight: bold; color: #FFF;");
        QVBoxLayout *iconLayout = new QVBoxLayout();
        iconLayout->addWidget(iconButton, 0, Qt::AlignCenter);
        iconLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
        QWidget *iconContainer = new QWidget();
        iconContainer->setLayout(iconLayout);
        iconGrid->addWidget(iconContainer, row, col);
        // Store info in button property for click
        iconButton->setProperty("achievementId", info.id);
        connect(iconButton, &QPushButton::clicked, this, [this, info]() {
            if (achievementTracker->isAchievementUnlocked(info.id)) {
                m_achievementDescriptionLabel->setText(QString("<b>%1:</b> %2").arg(info.name).arg(info.description));
            } else {
                m_achievementDescriptionLabel->setText(QString("<b>%1:</b> %2 (Progress: %3/%4)")
                                                     .arg(info.name)
                                                     .arg(info.description)
                                                     .arg(achievementTracker->getAchievementProgress(info.id))
                                                     .arg(info.requiredValue));
            }
            m_achievementDescriptionLabel->setStyleSheet("font-style: normal; color: #FFF;");
        });
        col++;
        if (col >= columns) { col = 0; row++; }
    }
    achievementsLayout->addWidget(iconGridWidget, 0, Qt::AlignCenter);

    // Stats labels
    totalAchievementsLabel = new QLabel("Total Achievements: 0");
    unlockedAchievementsLabel = new QLabel("Unlocked: 0");
    achievementCompletionPercentageLabel = new QLabel("Completion: 0.0%");
    achievementsLayout->addWidget(totalAchievementsLabel);
    achievementsLayout->addWidget(unlockedAchievementsLabel);
    achievementsLayout->addWidget(achievementCompletionPercentageLabel);

    // Description label
    m_achievementDescriptionLabel = new QLabel("Click an achievement to see its description.");
    m_achievementDescriptionLabel->setWordWrap(true);
    m_achievementDescriptionLabel->setAlignment(Qt::AlignCenter);
    m_achievementDescriptionLabel->setStyleSheet("font-style: italic; color: #888;");
    achievementsLayout->addWidget(m_achievementDescriptionLabel);

    layout->addWidget(achievementsGroup);
    tabWidget->addTab(achievementsTab, "Achievements");

    // Update stats
    totalAchievementsLabel->setText(QString("Total Achievements: %1").arg(achievementTracker->getTotalAchievements()));
    unlockedAchievementsLabel->setText(QString("Unlocked: %1").arg(achievementTracker->getUnlockedCount()));
    achievementCompletionPercentageLabel->setText(QString("Completion: %1%")
        .arg(achievementTracker->getCompletionPercentage(), 0, 'f', 1));
}

MainWindow::~MainWindow()
{
    if (webcamActive) {
        stopWebcam();
    }
    
    // Clean up chart components
    delete focusChartView;
    delete blinkChartView;
    delete focusSeries;
    delete blinkSeries;
    delete timeAxis;
    delete focusAxis;
    delete blinkAxis;
    delete ui;
}

bool MainWindow::loadFaceClassifier()
{
    QStringList paths = {
        QCoreApplication::applicationDirPath() + "/haarcascade_frontalface_alt.xml",
        QCoreApplication::applicationDirPath() + "/resources/haarcascade_frontalface_alt.xml",
        "haarcascade_frontalface_alt.xml",
        "resources/haarcascade_frontalface_alt.xml",
        "../resources/haarcascade_frontalface_alt.xml"
    };

    bool faceLoaded = false;
    for (const QString &path : paths) {
        QFileInfo file(path);
        if (file.exists() && file.isFile()) {
            if (faceCascade.load(path.toStdString())) {
                qDebug() << "Loaded face classifier from:" << path;
                faceLoaded = true;
                break;
            }
        }
    }

    if (!faceLoaded) {
        qDebug() << "Failed to load face classifier!";
        return false;
    }

    // multiple paths for eye classifier
    QStringList eyePaths = {
        QCoreApplication::applicationDirPath() + "/haarcascade_eye.xml",
        QCoreApplication::applicationDirPath() + "/resources/haarcascade_eye.xml",
        "haarcascade_eye.xml",
        "resources/haarcascade_eye.xml",
        "../resources/haarcascade_eye.xml"
    };

    bool eyeLoaded = false;
    for (const QString &path : eyePaths) {
        QFileInfo file(path);
        if (file.exists() && file.isFile()) {
            if (eyeCascade.load(path.toStdString())) {
                qDebug() << "Loaded eye classifier from:" << path;
                eyeLoaded = true;
                break;
            }
        }
    }

    if (!eyeLoaded) {
        qDebug() << "Failed to load eye classifier!";
        return false;
    }

    return true;
}

void MainWindow::startWebcam()
{
    qDebug() << "startWebcam: called";
    // Goal selection dialog before starting webcam session
    QList<int> selectedGoalIds;
    QDialog goalDialog(this);
    goalDialog.setWindowTitle("Select Goals for this Session");
    QVBoxLayout *goalLayout = new QVBoxLayout(&goalDialog);
    QLabel *goalLabel = new QLabel("Select one or more goals to associate with this camera session:");
    goalLayout->addWidget(goalLabel);
    QListWidget *goalList = new QListWidget();
    goalList->setSelectionMode(QAbstractItemView::MultiSelection);
    QList<GoalInfo> allGoals = studyGoal->getGoalsForDate(QDate::currentDate());
    for (const auto &goal : allGoals) {
        int completed = goal.completedMinutes;
        int target = goal.targetMinutes > 0 ? goal.targetMinutes : 1;
        QListWidgetItem *item = new QListWidgetItem(goal.subject + " (" + goal.category + ")");
        item->setData(Qt::UserRole, goal.id);
        if (target > 0 && completed >= target) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setText(item->text() + " [Completed]");
        }
        goalList->addItem(item);
    }
    goalLayout->addWidget(goalList);
    QDialogButtonBox *goalBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    goalLayout->addWidget(goalBox);
    connect(goalBox, &QDialogButtonBox::accepted, &goalDialog, &QDialog::accept);
    connect(goalBox, &QDialogButtonBox::rejected, &goalDialog, &QDialog::reject);
    if (goalDialog.exec() != QDialog::Accepted) {
        qDebug() << "startWebcam: goal dialog cancelled";
        return;
    }
    for (QListWidgetItem *item : goalList->selectedItems()) {
        selectedGoalIds.append(item->data(Qt::UserRole).toInt());
    }
    // End goal selection dialog

    if (!capture.open(0)) {
        QMessageBox::critical(this, "Error", "Could not open the webcam!");
        qDebug() << "startWebcam: failed to open webcam";
        return;
    }
    qDebug() << "startWebcam: webcam opened";

    // Create a new study session record for this webcam session
    int sessionId = studySession ? studySession->createSession(-1, "webcam", "Webcam session started") : -1;
    if (studySession) studySession->setCurrentSessionId(sessionId);
    qDebug() << "startWebcam: sessionId=" << sessionId;

    // Link selected goals to this session
    for (int goalId : selectedGoalIds) {
        QSqlQuery linkQuery(db);
        linkQuery.prepare("INSERT INTO session_goals (session_id, goal_id) VALUES (?, ?)");
        linkQuery.addBindValue(sessionId);
        linkQuery.addBindValue(goalId);
        linkQuery.exec();
    }
    // End goal linking

    sessionTimer.start();
    totalFrames = 0;
    focusedFrames = 0;
    blinkCount = 0;
    consecutiveNoEyeFrames = 0;
    eyesPreviouslyDetected = false;

    clearCharts();

    webcamActive = true;
    if (timer) {
    timer->start(33);
        qDebug() << "startWebcam: timer started";
    } else {
        qDebug() << "startWebcam: timer is null!";
    }
    if (startButton) startButton->setEnabled(false);
    else qDebug() << "startWebcam: startButton is null!";
    if (stopButton) stopButton->setEnabled(true);
    else qDebug() << "startWebcam: stopButton is null!";
    qDebug() << "startWebcam: finished";
}

void MainWindow::stopWebcam()
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "Failed to reopen database in stopWebcam:" << db.lastError().text();
            return;
        }
    }
    timer->stop();
    capture.release();
    webcamActive = false;
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    webcamLabel->clear();
    webcamLabel->setText("Webcam feed will appear here");
    int durationSeconds = sessionTimer.elapsed() / 1000;
    float focusScore = totalFrames > 0 ? (float)focusedFrames / totalFrames : 0.0f;

    qDebug() << "Session Duration (s):" << durationSeconds;
    qDebug() << "Focus Score:" << focusScore;
    qDebug() << "Total Blinks:" << blinkCount;

    // Emit session completed signal for achievement tracking
    achievementTracker->sessionCompleted();

    // Log final session summary
    QSqlQuery query;
    query.prepare("INSERT INTO detections (timestamp, face_count, eyes_detected, blink_count, focus_score) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    query.addBindValue(focusedFrames);
    query.addBindValue(true);
    query.addBindValue(blinkCount);
    query.addBindValue(focusScore);

    if (!query.exec()) {
        qDebug() << "Error: Failed to log session summary" << query.lastError().text();
    } else {
        qDebug() << "Successfully logged session summary with focus score:" << focusScore;
    }

    int durationMinutes = sessionTimer.elapsed() / 60000;
    studyStreak->recordStudySession(durationMinutes);

    // End the study session record
    int sessionId = studySession->getCurrentSessionId();
    if (sessionId != -1) {
        studySession->endSession(sessionId);
        studySession->setCurrentSessionId(-1);

        //Update progress for all linked goals
        QSqlQuery linkQuery(db);
        linkQuery.prepare("SELECT goal_id FROM session_goals WHERE session_id = ?");
        linkQuery.addBindValue(sessionId);
        if (linkQuery.exec()) {
            while (linkQuery.next()) {
                int goalId = linkQuery.value(0).toInt();
                qDebug() << "Adding progress to goal" << goalId << "for duration" << durationMinutes << "minutes (camera session)";
                studyGoal->addProgress(goalId, durationMinutes);
            }
        }

    }
}

void MainWindow::updateFrame()
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "Failed to reopen database in updateFrame:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "updateFrame: called";
    if (!capture.isOpened()) {
        qDebug() << "updateFrame: capture not opened";
        return;
    }
    capture.read(frame);
    if (frame.empty()) {
        QMessageBox::warning(this, "Warning", "Empty frame received from webcam!");
        stopWebcam();
        qDebug() << "updateFrame: empty frame";
        return;
    }

    totalFrames++;

    // detect faces and eyes
    if (faceDetectionEnabled) {
        detectAndDisplayFaces(frame);
    }

    // Update blink count label after processing the frame
    if (blinkCountLabel) {
        blinkCountLabel->setText(QString("Blinks: %1").arg(blinkCount));
    } else {
        qDebug() << "updateFrame: blinkCountLabel is null!";
    }

    displayImage(frame);
    qDebug() << "updateFrame: finished";
}

void MainWindow::detectAndDisplayFaces(cv::Mat &frame)
{
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    // reduce false positives
    faceCascade.detectMultiScale(gray, faces, 1.2, 5, 0, cv::Size(60, 60), cv::Size(500, 500));

    int faceCount = static_cast<int>(faces.size());
    bool currentEyesDetected = false;

    if (faceCount > 0 && faceCount < 5) {
        focusedFrames++;
    }

    for (const auto &face : faces) {
        // Draw face rectangle
        cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);
        cv::putText(frame, "Face Detected", cv::Point(face.x, face.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);

        // Region of interest (ROI) for the face
        cv::Mat faceROI = gray(face);
        // Focus on upper half of face for better eye detection
        cv::Mat upperFaceROI = faceROI(cv::Rect(0, 0, face.width, face.height / 2));

        std::vector<cv::Rect> eyes;
        eyeCascade.detectMultiScale(upperFaceROI, eyes, 1.1, 3, 0, cv::Size(15, 15));

        if (!eyes.empty()) {
            qDebug() << "Eyes detected:" << eyes.size() << "in face region";
        } else {
            qDebug() << "No eyes detected in face region";
        }

        // If eyes are detected, draw them
        for (const auto &eye : eyes) {
            float aspectRatio = static_cast<float>(eye.width) / static_cast<float>(eye.height);
            if (aspectRatio > 0.5 && aspectRatio < 5.0) {
                cv::Point center(face.x + eye.x + eye.width / 2, face.y + eye.y + eye.height / 2);
                int radius = std::round((eye.width + eye.height) * 0.25);
                cv::circle(frame, center, radius, cv::Scalar(0, 0, 255), 2);

                // Add text indicating eyes detected
                cv::putText(frame, "Eye",
                            cv::Point(center.x - radius, center.y - radius - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
            }
        }

        if (!eyes.empty()) {
            currentEyesDetected = true;
        }
    }

    // Blink detection logic
    if (eyesPreviouslyDetected && !currentEyesDetected) {
        consecutiveNoEyeFrames = 1;
    } else if (!eyesPreviouslyDetected && !currentEyesDetected) {
        consecutiveNoEyeFrames++;
    } else if (!eyesPreviouslyDetected && currentEyesDetected) {
        if (consecutiveNoEyeFrames >= 1 && consecutiveNoEyeFrames <= 3) {
            blinkCount++;
            qDebug() << "Blink detected! Total blinks:" << blinkCount;

            // Add visual indication of blink
            cv::putText(frame, "BLINK DETECTED",
                        cv::Point(frame.cols/2 - 100, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
        }
        consecutiveNoEyeFrames = 0;
    }

    eyesPreviouslyDetected = currentEyesDetected;

    // Add focus status indicator to the frame
    float focusScore = totalFrames > 0 ? (float)focusedFrames / totalFrames : 0.0f;
    cv::Scalar focusColor;
    std::string focusText;

    if (focusScore > 0.8) {
        focusColor = cv::Scalar(0, 255, 0);  // Green
        focusText = "Focused";
    } else if (focusScore > 0.5) {
        focusColor = cv::Scalar(0, 255, 255);  // Yellow
        focusText = "Partially Focused";
    } else {
        focusColor = cv::Scalar(0, 0, 255);  // Red
        focusText = "Not Focused";
    }

    cv::putText(frame, focusText, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, focusColor, 2);

    // Display focus percentage
    cv::putText(frame,
                "Focus: " + std::to_string(static_cast<int>(focusScore * 100)) + "%",
                cv::Point(10, 60),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, focusColor, 2);

    // Log the face detection event to the database
    logFaceDetection(faceCount, currentEyesDetected, focusScore);
}

void MainWindow::displayImage(const cv::Mat &frame)
{
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    webcamLabel->setPixmap(QPixmap::fromImage(qimg).scaled(webcamLabel->size(), Qt::KeepAspectRatio));
}

void MainWindow::logFaceDetection(int faceCount, bool eyesDetected, float focusScore)
{
    if (!db.isOpen()) {
        qDebug() << "Error: Database is not open when trying to log detection";
        return;
    }

    QSqlQuery query(db);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // Updated query
    query.prepare("INSERT INTO detections (timestamp, face_count, eyes_detected, blink_count, focus_score) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(timestamp);
    query.addBindValue(faceCount);
    query.addBindValue(eyesDetected);
    query.addBindValue(blinkCount);
    query.addBindValue(focusScore);

    if (!query.exec()) {
        qDebug() << "Error: Failed to log face detection" << query.lastError().text();
        qDebug() << "Query details - timestamp:" << timestamp
                 << "faces:" << faceCount
                 << "eyes:" << eyesDetected
                 << "blinks:" << blinkCount
                 << "focus:" << focusScore;
    } else {
        qDebug() << "Successfully logged detection - Timestamp:" << timestamp
                 << "Faces:" << faceCount
                 << "Eyes:" << (eyesDetected ? "Yes" : "No")
                 << "Blinks:" << blinkCount
                 << "Focus:" << (focusScore * 100) << "%";
    }
}

void MainWindow::showDetectionHistory()
{
    QSqlQuery query(db);

    if (!db.isOpen()) {
        QMessageBox::critical(this, "Database Error", "Database is not open!");
        return;
    }

    if (!query.exec("SELECT timestamp, face_count, eyes_detected, blink_count, focus_score FROM detections ORDER BY timestamp DESC LIMIT 100")) {
        QMessageBox::critical(this, "Query Error",
                              "Failed to fetch detection history: " + query.lastError().text());
        qDebug() << "Query error:" << query.lastError().text();
        return;
    }

    QString history = "Face Detection History:\n\n";
    int recordCount = 0;

    // Get statistics
    QSqlQuery statsQuery(db);
    if (statsQuery.exec("SELECT AVG(focus_score) as avg_focus, AVG(blink_count) as avg_blinks FROM detections")) {
        if (statsQuery.next()) {
            double avgFocus = statsQuery.value(0).toDouble() * 100;
            double avgBlinks = statsQuery.value(1).toDouble();
            history += QString("Average Focus: %1%  |  Average Blinks: %2\n")
                          .arg(QString::number(avgFocus, 'f', 1))
                          .arg(QString::number(avgBlinks, 'f', 1));
            history += "------------------------------------------------\n\n";
        }
    }

    while (query.next()) {
        recordCount++;
        QString timestamp = query.value(0).toString();
        int faceCount = query.value(1).toInt();
        bool eyesDetected = query.value(2).toBool();
        int blinkCount = query.value(3).toInt();
        float focusScore = query.value(4).toFloat() * 100;

        QString focusBar;
        int barLength = static_cast<int>(focusScore / 5);
        for (int i = 0; i < barLength; ++i) {
            focusBar += "█";
        }

        QDateTime sessionTime = QDateTime::fromString(timestamp, "yyyy-MM-dd HH:mm:ss");
        QString timeDisplay;
        if (sessionTime.date() == QDate::currentDate()) {
            timeDisplay = sessionTime.toString("HH:mm:ss");
        } else {
            timeDisplay = sessionTime.toString("MM-dd HH:mm");
        }

        history += QString("%1\n").arg(timeDisplay);
        history += QString("  Focus: %1% %2\n")
                      .arg(QString::number(focusScore, 'f', 1), 5)
                      .arg(focusBar);
        history += QString("  Faces: %1 | Eyes: %2 | Blinks: %3\n")
                       .arg(faceCount)
                      .arg(eyesDetected ? "✓" : "✗")
                      .arg(blinkCount);
        history += "------------------------------------------------\n";
    }

    if (recordCount == 0) {
        history = "No detection records found in the database.";
    }

    // Use a QDialog with QTextEdit for scrollable history
    QDialog dialog(this);
    dialog.setWindowTitle("Detection History");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Courier New", 10));
    textEdit->setText(history);
    layout->addWidget(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog.resize(600, 600);
    dialog.exec();
}

void MainWindow::loadSettings()
{
    QSettings settings("StudyBuddy", "FocusMonitor");
    alertsEnabled = settings.value("alertsEnabled", true).toBool();
    alertThreshold = settings.value("alertThreshold", 5).toInt();
    faceDetectionEnabled = settings.value("faceDetectionEnabled", true).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings("StudyBuddy", "FocusMonitor");
    settings.setValue("alertsEnabled", alertsEnabled);
    settings.setValue("alertThreshold", alertThreshold);
    settings.setValue("faceDetectionEnabled", faceDetectionEnabled);
}

void MainWindow::playAlertSound()
{
    if (alertsEnabled && alertSound->status() == QSoundEffect::Ready) {
        alertSound->play();
    }
}

void MainWindow::exportData()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export Data"), "",
        tr("CSV Files (*.csv);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"),
                            tr("Could not open file for writing."));
        return;
    }

    QTextStream out(&file);
    // Write CSV header
    out << "Timestamp,Face Count,Eyes Detected,Blink Count,Focus Score\n";

    QSqlQuery query;
    query.exec("SELECT timestamp, face_count, eyes_detected, blink_count, focus_score "
              "FROM detections ORDER BY timestamp DESC");

    while (query.next()) {
        out << query.value(0).toString() << ","
            << query.value(1).toString() << ","
            << (query.value(2).toBool() ? "Yes" : "No") << ","
            << query.value(3).toString() << ","
            << QString::number(query.value(4).toDouble() * 100, 'f', 2) << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Success"),
                           tr("Data exported successfully to %1").arg(fileName));
}

void MainWindow::showSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Settings"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Alert settings
    QGroupBox *alertGroup = new QGroupBox(tr("Alert Settings"), &dialog);
    QVBoxLayout *alertLayout = new QVBoxLayout(alertGroup);

    QCheckBox *enableAlertsCheck = new QCheckBox(tr("Enable Focus Alerts"), &dialog);
    enableAlertsCheck->setChecked(alertsEnabled);

    QSpinBox *thresholdSpin = new QSpinBox(&dialog);
    thresholdSpin->setRange(1, 30);
    thresholdSpin->setValue(alertThreshold);
    thresholdSpin->setSuffix(tr(" seconds"));

    alertLayout->addWidget(enableAlertsCheck);
    alertLayout->addWidget(new QLabel(tr("Alert Threshold:")));
    alertLayout->addWidget(thresholdSpin);

    // Face Detection Settings
    QGroupBox *faceGroup = new QGroupBox(tr("Face Detection"), &dialog);
    QVBoxLayout *faceLayout = new QVBoxLayout(faceGroup);

    QCheckBox *enableFaceCheck = new QCheckBox(tr("Enable Face Detection"), &dialog);
    enableFaceCheck->setChecked(faceDetectionEnabled);

    faceLayout->addWidget(enableFaceCheck);

    // Add groups to main layout
    layout->addWidget(alertGroup);
    layout->addWidget(faceGroup);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, &dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        alertsEnabled = enableAlertsCheck->isChecked();
        alertThreshold = thresholdSpin->value();
        faceDetectionEnabled = enableFaceCheck->isChecked();
        saveSettings();
    }
}

void MainWindow::updateDashboard()
{
    if (!webcamActive)
        return;

    // Update session duration
    int elapsed = sessionTimer.elapsed() / 1000;
    int hours = elapsed / 3600;
    int minutes = (elapsed % 3600) / 60;
    int seconds = elapsed % 60;
    sessionDurationLabel->setText(QString("Duration: %1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));

    // Update focus score
    float focusScore = totalFrames > 0 ? (float)focusedFrames / totalFrames * 100 : 0.0f;
    focusScoreLabel->setText(QString("Focus Score: %1%").arg(QString::number(focusScore, 'f', 1)));
    focusProgressBar->setValue(static_cast<int>(focusScore));

    // Update charts
    updateChartData();

    // Check if focus is too low and show alert if needed
    if (alertsEnabled && focusScore < 50.0f) {
        static QTime lastAlertTime = QTime::currentTime();
        QTime currentTime = QTime::currentTime();
        if (lastAlertTime.secsTo(currentTime) >= alertThreshold) {
            showFocusAlert();
            lastAlertTime = currentTime;
        }
    }
}

void MainWindow::showFocusAlert()
{
    if (!alertsEnabled)
        return;

    // Play alert sound
    playAlertSound();

    // Show system tray notification
    if (trayIcon && QSystemTrayIcon::supportsMessages()) {
        trayIcon->showMessage(
            tr("Focus Alert"),
            tr("Your focus level is low. Please pay attention to your work."),
            QSystemTrayIcon::Warning,
            3000
        );
    }

    // Show visual alert on the webcam feed
    if (webcamActive && !frame.empty()) {
        cv::putText(frame, "LOW FOCUS ALERT!",
                    cv::Point(frame.cols/2 - 100, frame.rows/2),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0,
                    cv::Scalar(0, 0, 255), 2);
        displayImage(frame);
    }
}

void MainWindow::setupCharts()
{
    try {
        // Focus Chart
        focusChart = new QChart();
        focusSeries = new QLineSeries();
        if (!focusChart || !focusSeries) {
            qDebug() << "Failed to create focus chart components";
            return;
        }
        
        focusChart->addSeries(focusSeries);
        focusChart->setTitle("Focus Level Over Time");
        
        timeAxis = new QValueAxis();
        focusAxis = new QValueAxis();
        if (!timeAxis || !focusAxis) {
            qDebug() << "Failed to create focus chart axes";
            return;
        }
        
        timeAxis->setTitleText("Time (minutes)");
        timeAxis->setLabelFormat("%.1f");
        timeAxis->setTickCount(6);
        
        focusAxis->setTitleText("Focus Score (%)");
        focusAxis->setRange(0, 100);
        focusAxis->setTickCount(5);
        
        focusChart->addAxis(timeAxis, Qt::AlignBottom);
        focusChart->addAxis(focusAxis, Qt::AlignLeft);
        focusSeries->attachAxis(timeAxis);
        focusSeries->attachAxis(focusAxis);
        
        focusChartView = new QChartView(focusChart);
        if (!focusChartView) {
            qDebug() << "Failed to create focus chart view";
            return;
        }
        focusChartView->setRenderHint(QPainter::Antialiasing);

        // Blink Chart
        blinkChart = new QChart();
        blinkSeries = new QLineSeries();
        if (!blinkChart || !blinkSeries) {
            qDebug() << "Failed to create blink chart components";
            return;
        }
        
        blinkChart->addSeries(blinkSeries);
        blinkChart->setTitle("Blink Rate Over Time");
        
        blinkAxis = new QValueAxis();
        if (!blinkAxis) {
            qDebug() << "Failed to create blink chart axis";
            return;
        }
        
        QValueAxis *blinkTimeAxis = new QValueAxis();
        blinkTimeAxis->setTitleText("Time (minutes)");
        blinkTimeAxis->setLabelFormat("%.1f");
        blinkTimeAxis->setTickCount(6);
        
        blinkAxis->setTitleText("Blinks per Minute");
        blinkAxis->setRange(0, 30);
        blinkAxis->setTickCount(6);
        
        blinkChart->addAxis(blinkTimeAxis, Qt::AlignBottom);
        blinkChart->addAxis(blinkAxis, Qt::AlignLeft);
        blinkSeries->attachAxis(blinkTimeAxis);
        blinkSeries->attachAxis(blinkAxis);
        
        blinkChartView = new QChartView(blinkChart);
        if (!blinkChartView) {
            qDebug() << "Failed to create blink chart view";
            return;
        }
        blinkChartView->setRenderHint(QPainter::Antialiasing);

        // Set minimum sizes to prevent layout issues
        focusChartView->setMinimumSize(400, 300);
        blinkChartView->setMinimumSize(400, 300);

        // Set chart themes for better visibility
        focusChart->setTheme(QChart::ChartThemeDark);
        blinkChart->setTheme(QChart::ChartThemeDark);

        // Enable chart animations
        focusChart->setAnimationOptions(QChart::SeriesAnimations);
        blinkChart->setAnimationOptions(QChart::SeriesAnimations);

    } catch (const std::exception& e) {
        qDebug() << "Exception in setupCharts:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in setupCharts";
    }
}

void MainWindow::updateChartData()
{
    if (!focusSeries || !blinkSeries || !timeAxis || !focusAxis || !blinkAxis) {
        qDebug() << "Chart components not initialized";
        return;
    }

    try {
        // Get elapsed time in minutes
        double elapsedMinutes = sessionTimer.elapsed() / 60000.0;
        
        // Calculate current focus score as percentage
        currentFocusScore = totalFrames > 0 ? (float)focusedFrames / totalFrames * 100.0f : 0.0f;
        
        // Calculate blink rate (blinks per minute)
        static int lastBlinkCount = 0;
        blinkRate = blinkCount - lastBlinkCount;
        lastBlinkCount = blinkCount;
        
        // Add new data points
        focusSeries->append(elapsedMinutes, currentFocusScore);
        blinkSeries->append(elapsedMinutes, blinkRate);

        // Keep only last hour of data (60 minutes)
        while (focusSeries->count() > 0 && 
               focusSeries->at(0).x() < elapsedMinutes - 60.0) {
            focusSeries->remove(0);
        }
        while (blinkSeries->count() > 0 && 
               blinkSeries->at(0).x() < elapsedMinutes - 60.0) {
            blinkSeries->remove(0);
        }

        // Update axes ranges
        if (focusSeries->count() > 0) {
            double minTime = focusSeries->at(0).x();
            double maxTime = qMax(minTime + 1.0, elapsedMinutes);
            timeAxis->setRange(minTime, maxTime);
            focusAxis->setRange(0, 100);
            blinkAxis->setRange(0, qMax(30.0, blinkRate * 1.2));
        }

        // Update focus progress bar
        if (focusProgressBar) {
            focusProgressBar->setValue(static_cast<int>(currentFocusScore));
        }

        // update of chart views
        if (focusChartView) focusChartView->update();
        if (blinkChartView) blinkChartView->update();

        qDebug() << "Chart updated - Time:" << elapsedMinutes 
                 << "Focus Score:" << currentFocusScore 
                 << "Blink Rate:" << blinkRate;
    }
    catch (const std::exception& e) {
        qDebug() << "Error updating charts:" << e.what();
    }
}

void MainWindow::clearCharts()
{
    if (!focusSeries || !blinkSeries) {
        qDebug() << "clearCharts: Chart series not initialized";
        return;
    }

    try {
        focusSeries->clear();
        blinkSeries->clear();
        sessionTimer.restart();
        // Reset axes
        if (timeAxis) timeAxis->setRange(0, 1);
        else qDebug() << "clearCharts: timeAxis is null!";
        if (focusAxis) focusAxis->setRange(0, 100);
        else qDebug() << "clearCharts: focusAxis is null!";
        if (blinkAxis) blinkAxis->setRange(0, 30);
        else qDebug() << "clearCharts: blinkAxis is null!";
        // Force update
        if (focusChartView) focusChartView->update();
        else qDebug() << "clearCharts: focusChartView is null!";
        if (blinkChartView) blinkChartView->update();
        else qDebug() << "clearCharts: blinkChartView is null!";
    }
    catch (const std::exception& e) {
        qDebug() << "Error clearing charts:" << e.what();
    }
}

// Pomodoro Slots Implementation
void MainWindow::updatePomodoroDisplay(const QString &time)
{
    if (pomodoroTimeLabel) {
        pomodoroTimeLabel->setText(time);
    }
    // Real-time update of active goal progress bar
    if (m_activeGoalProgressBar && m_currentActiveGoalId != -1) {
        // Do NOT increment m_currentSessionProgress here. Only use the value set by onGoalSessionTimeUpdated.
        int completed = studyGoal->getProgress(m_currentActiveGoalId);
        int target = studyGoal->getTarget(m_currentActiveGoalId);
        if (target <= 0) target = 1;
        int elapsedSeconds = m_currentSessionProgress.value(m_currentActiveGoalId, 0);
        int elapsedMinutes = elapsedSeconds / 60;
        int total = completed + elapsedMinutes;
        float percent = (target > 0) ? (float)total / target * 100.0f : 0.0f;
        m_activeGoalProgressBar->setRange(0, target);
        m_activeGoalProgressBar->setValue(total);
        m_activeGoalProgressBar->setFormat(QString("%1/%2 minutes (%3%)").arg(total).arg(target).arg(percent, 0, 'f', 1));
    }
}

void MainWindow::handlePomodoroModeChanged(bool isStudyTime)
{
    if (pomodoroModeLabel) {
        pomodoroModeLabel->setText(QString("Mode: %1").arg(isStudyTime ? "Study" : "Break"));
    }
}

void MainWindow::handlePomodoroCycleChanged(int cycle)
{
    if (pomodoroCycleLabel) {
        pomodoroCycleLabel->setText(QString("Cycle: %1").arg(cycle));
    }
}

void MainWindow::handlePomodoroTimerCompleted()
{
    // Emit Pomodoro completed signal for achievement tracking
    achievementTracker->pomodoroCompleted();

    // Desktop notification for Pomodoro completion
    if (trayIcon && QSystemTrayIcon::supportsMessages() && enableDesktopNotif->isChecked() && notifPomodoro->isChecked()) {
        trayIcon->showMessage(tr("Pomodoro Complete"), tr("Great job! Take a break."), QSystemTrayIcon::Information, 3000);
    }

    // You can add a sound effect or a message box here
    QMessageBox::information(this, "Pomodoro", "Timer completed!");
}

void MainWindow::handleAddGoal()
{
}

void MainWindow::refreshGoalsDisplay()
{
    // Clear old cards
    QLayoutItem *child;
    while ((child = goalsCardLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    m_activeGoalProgressBar = nullptr; // Reset before rebuilding
    QList<GoalInfo> goals = studyGoal->getGoalsForDate(QDate::currentDate());
    for (const auto& goal : goals) {
        qDebug() << "Goal" << goal.id << "completed:" << goal.completedMinutes << "target:" << goal.targetMinutes;
    }

    if (goals.isEmpty()) {
        QLabel *emptyLabel = new QLabel("No goals yet! Create your first goal above.");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #aaa; font-style: italic;");
        goalsCardLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int colCount = 3; // Number of cards per row
    int row = 0, col = 0;
    for (const auto &goal : goals) {
        QWidget *card = new QWidget();
        QVBoxLayout *cardLayout = new QVBoxLayout(card);

        // --- Resource emoji row ---
        QHBoxLayout *resourceEmojiLayout = new QHBoxLayout();
        resourceEmojiLayout->addStretch();
        if (!goal.resources.isEmpty()) {
            QPushButton *resourceBtn = new QPushButton();
            resourceBtn->setFlat(true);
            resourceBtn->setCursor(Qt::PointingHandCursor);
            resourceBtn->setStyleSheet("QPushButton { background: transparent; font-size: 20px; } QPushButton:hover { color: #C3073F; }");
            // Show only one emoji: 📎 if any file, otherwise 🔗 if any link
            bool hasFile = false, hasLink = false;
            for (const QString &res : goal.resources) {
                if (res.startsWith("http")) hasLink = true;
                else hasFile = true;
            }
            QString emoji;
            if (hasFile) emoji = "📎";
            else emoji = "🔗";
            resourceBtn->setText(emoji);
            resourceEmojiLayout->addWidget(resourceBtn, 0, Qt::AlignRight);
            // Popup menu on click
            connect(resourceBtn, &QPushButton::clicked, this, [this, goal, resourceBtn]() {
                QMenu *menu = new QMenu(resourceBtn);
                for (const QString &res : goal.resources) {
                    QAction *act = new QAction(res, menu);
                    connect(act, &QAction::triggered, this, [this, res]() {
                        openResourceInApp(res);
                    });
                    menu->addAction(act);
                }
                menu->exec(QCursor::pos());
            });
        }
        cardLayout->addLayout(resourceEmojiLayout);

        // Title, category, and recurrence (all in one line)
        QString titleText = QString("%1 (%2)").arg(goal.subject, goal.category);
        if (goal.recurrenceType != "None") {
            titleText += QString("  <span style='background:#6F2232; color:#fff; border-radius:4px; padding:2px 6px; font-size:12px;'>[%1: %2]</span>")
                .arg(goal.recurrenceType, goal.recurrenceValue);
        }
        QLabel *title = new QLabel(titleText);
        title->setStyleSheet("font-weight: bold; font-size: 16px;");
        title->setTextFormat(Qt::RichText);
        cardLayout->addWidget(title);

        // Progress bar
        int completed = goal.completedMinutes;
        int target = goal.targetMinutes > 0 ? goal.targetMinutes : 1; // Prevent zero range
        float percent = (target > 0) ? (float)completed / target * 100.0f : 0.0f;
        QProgressBar *progress = new QProgressBar();
        progress->setObjectName("goalProgressBar");
        progress->setRange(0, target);
        progress->setValue(completed);
        progress->setFormat(QString("%1/%2 minutes (%3%)").arg(completed).arg(target).arg(percent, 0, 'f', 1));
        progress->setStyleSheet("QProgressBar::chunk { background-color: #C3073F; min-width: 2px; }"); // Always visible
        cardLayout->addWidget(progress);
        if (goal.id == m_currentActiveGoalId) {
            m_activeGoalProgressBar = progress;
        }

        // Inline actions
        QHBoxLayout *actions = new QHBoxLayout();
        QPushButton *startBtn = new QPushButton("Start");
        startBtn->setToolTip("Start Tracking");
        QPushButton *stopBtn = new QPushButton("Stop");
        stopBtn->setToolTip("Stop Tracking");
        QPushButton *deleteBtn = new QPushButton("Delete");
        deleteBtn->setToolTip("Delete Goal");
        actions->addWidget(startBtn);
        actions->addWidget(stopBtn);
        actions->addWidget(deleteBtn);
        QString btnStyle = "QPushButton { background-color: #6F2232; color: white; border: none; padding: 5px 10px; border-radius: 3px;width: 100px; } QPushButton:hover { background-color: #950740; }";
        startBtn->setStyleSheet(btnStyle);
        stopBtn->setStyleSheet(btnStyle);
        deleteBtn->setStyleSheet(btnStyle);
        actions->addStretch();
        cardLayout->addLayout(actions);

        connect(startBtn, &QPushButton::clicked, this, [this, goal]() {
            if (!pomodoroTimer || !studyGoal) {
                qDebug() << "Error: pomodoroTimer or studyGoal is null!";
                return;
            }
            // Prevent tracking if goal is already completed
            int completed = studyGoal->getProgress(goal.id);
            int target = studyGoal->getTarget(goal.id);
            if (target > 0 && completed >= target) {
                QMessageBox::warning(this, "Goal Already Completed", "You cannot track a goal that is already completed.");
                return;
            }
            // Clear session progress for all other goals
            m_currentSessionProgress.clear();
            m_currentActiveGoalId = goal.id;
            int initialElapsedSeconds = 0;
            studyGoal->startTrackingGoal(goal.id, initialElapsedSeconds);
            pomodoroTimer->start();
        });
        connect(stopBtn, &QPushButton::clicked, this, [this]() {
            if (!pomodoroTimer || !studyGoal) {
                qDebug() << "Error: pomodoroTimer or studyGoal is null!";
                return;
            }
            qDebug() << "Stop button clicked";
            pomodoroTimer->pause();
            studyGoal->stopTrackingGoal();
        });
        connect(deleteBtn, &QPushButton::clicked, this, [this, goal, card]() {
            if (QMessageBox::question(this, "Confirm Deletion", "Are you sure you want to delete this goal?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                studyGoal->deleteGoal(goal.id);
                card->deleteLater();
            }
        });

        card->setStyleSheet("background: #222; border: 1px solid #6F2232; border-radius: 8px; padding: 10px;");
        card->setFixedHeight(360);
        goalsCardLayout->addWidget(card, row, col);
        col++;
        if (col >= colCount) { col = 0; row++; }

        // --- Enable editing on card click ---
        card->setCursor(Qt::PointingHandCursor);
        card->installEventFilter(this);
        card->setProperty("goalId", goal.id);
    }
}
void MainWindow::handleGoalCreated(int goalId)
{
    refreshGoalsDisplay();
}

void MainWindow::handleGoalUpdated(int goalId)
{
    refreshGoalsDisplay();
}

void MainWindow::onGoalSessionTimeUpdated(int goalId, int elapsedSeconds)
{
    qDebug() << "MainWindow: Goal" << goalId << "session time updated:" << elapsedSeconds << "seconds";
    // Store the temporary session progress in seconds
    m_currentSessionProgress[goalId] = elapsedSeconds;
    // No more goalsListWidget updates here
}

void MainWindow::onGoalSessionProgressUpdated(int goalId, int sessionMinutes)
{
    qDebug() << "onGoalSessionProgressUpdated called for goal" << goalId << "sessionMinutes:" << sessionMinutes;
    m_currentSessionProgress.remove(goalId);
    refreshGoalsDisplay();

    // Check if goal is completed
    float completionPercentage = studyGoal->getCompletionPercentage(goalId);
    if (completionPercentage >= 100.0f) {
        // Stop Pomodoro if running
        if (pomodoroTimer && pomodoroTimer->isRunning()) {
            pomodoroTimer->pause();
        }
        // Stop webcam session if running
        if (webcamActive) {
            qDebug() << "Stopping webcam session because goal" << goalId << "is completed.";
            stopWebcam();
        }
        QMessageBox::information(this, "Goal Completed!",
                                 QString("Congratulations! You have completed your goal for '%1'!").arg(studyGoal->getGoalDetails(goalId)["subject"].toString()));
        refreshGoalsDisplay(); // Ensure UI updates after dialog
    }
}

void MainWindow::onGoalTrackingStarted(int goalId)
{
    Q_UNUSED(goalId);
    m_currentActiveGoalId = goalId;
    progressUpdateTimer->start();
    QMessageBox::information(this, "Goal Tracking", "Tracking started for selected goal.");
}

void MainWindow::onGoalTrackingStopped()
{
    progressUpdateTimer->stop();
}

void MainWindow::handleGoalDeleted(int goalId)
{
    Q_UNUSED(goalId);
    if (m_currentActiveGoalId == goalId) {
       // handleStopGoalTracking();
    }
    refreshGoalsDisplay();
}

void MainWindow::onAchievementUnlocked(const QString &achievementId, const QString &name)
{
    // Desktop notification for achievement unlock
    if (trayIcon && QSystemTrayIcon::supportsMessages()) {
        trayIcon->showMessage(tr("Achievement Unlocked!"), QString("Congratulations! You've unlocked the '%1' achievement!").arg(name), QSystemTrayIcon::Information, 3000);
    }
    QMessageBox::information(this, "Achievement Unlocked!", QString("Congratulations! You've unlocked the '%1' achievement!").arg(name));
    refreshAchievementsDisplay();
}

void MainWindow::onAchievementProgressUpdated(const QString &achievementId, int progress)
{
    Q_UNUSED(achievementId);
    Q_UNUSED(progress);
    refreshAchievementsDisplay();
}

void MainWindow::refreshAchievementsDisplay()
{
    totalAchievementsLabel->setText(QString("Total Achievements: %1").arg(achievementTracker->getTotalAchievements()));
    unlockedAchievementsLabel->setText(QString("Unlocked: %1").arg(achievementTracker->getUnlockedCount()));
    achievementCompletionPercentageLabel->setText(QString("Completion: %1%")
        .arg(achievementTracker->getCompletionPercentage(), 0, 'f', 1));
}

void MainWindow::handleCancelEdit()
{
    m_currentSelectedGoalId = -1;
    goalSubjectInput->clear();
    goalTargetMinutesInput->setValue(0);
    goalNotesInput->clear();

    recurrenceTypeCombo->setCurrentIndex(0);
    recurrenceValueInput->clear();
    goalCategoryCombo->setCurrentIndex(0);

    addGoalButton->setText("Add Goal");
    cancelEditButton->setEnabled(false);
    //startTrackingButton->setEnabled(false);
    //stopTrackingButton->setEnabled(false);
    //goalsListWidget->clearSelection();
}

void MainWindow::setupDarkAcademiaTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #1A1A1D;
        }
        
        QTabWidget::pane {
            border: 1px solid #4E4E50;
            background: #1A1A1D;
        }
        
        QTabBar::tab {
            background: #4E4E50;
            color: #D1D1D1;
            padding: 8px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        
        QTabBar::tab:selected {
            background: #6F2232;
            color: #FFFFFF;
        }
        
        QGroupBox {
            border: 1px solid #6F2232;
            border-radius: 5px;
            margin-top: 10px;
            color: #C5C6C7;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 3px;
            color: #C5C6C7;
        }
        
        QLabel {
            color: #C5C6C7;
        }
        
        QPushButton {
            background-color: #6F2232;
            color: white;
            border: none;
            padding: 5px 10px;
            border-radius: 3px;
        }
        
        QPushButton:hover {
            background-color: #950740;
        }
        
        QProgressBar {
            border: 1px solid #4E4E50;
            border-radius: 3px;
            text-align: center;
        }
        
        QProgressBar::chunk {
            background-color: #C3073F;
            width: 10px;
        }
    )";
    
    qApp->setStyleSheet(styleSheet);
}

float MainWindow::calculateHeadPose(const std::vector<cv::Point2f>& landmarks)
{
    if (landmarks.empty()) return 0.0f;
    if (landmarks.size() >= 2) {
        cv::Point2f leftEye = landmarks[0];
        cv::Point2f rightEye = landmarks[1];
        float dx = rightEye.x - leftEye.x;
        float dy = rightEye.y - leftEye.y;
        return atan2(dy, dx) * 180.0f / CV_PI;
    }
    return 0.0f;
}

void MainWindow::exportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export All Data"), "", tr("CSV Files (*.csv);;All Files (*)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open file for writing."));
        return;
    }
    QTextStream out(&file);

    // Export session_plans
    out << "Session Plans:\n";
    out << "ID,Subject,Goals,Resource Link,Mental State,Plan Date\n";
    QSqlQuery planQuery;
    planQuery.exec("SELECT id, subject, goals, resource_link, mental_state, plan_date FROM session_plans");
    while (planQuery.next()) {
        out << planQuery.value(0).toString() << ","
            << planQuery.value(1).toString() << ","
            << planQuery.value(2).toString() << ","
            << planQuery.value(3).toString() << ","
            << planQuery.value(4).toString() << ","
            << planQuery.value(5).toString() << "\n";
    }
    out << "\n";

    // Export session_reviews
    out << "Session Reviews:\n";
    out << "ID,Session Plan ID,Focus Score,Distraction Events,Effectiveness,Notes,Review Date\n";
    QSqlQuery reviewQuery;
    reviewQuery.exec("SELECT id, session_plan_id, focus_score, distraction_events, effectiveness, notes, review_date FROM session_reviews");
    while (reviewQuery.next()) {
        out << reviewQuery.value(0).toString() << ","
            << reviewQuery.value(1).toString() << ","
            << reviewQuery.value(2).toString() << ","
            << reviewQuery.value(3).toString() << ","
            << reviewQuery.value(4).toString() << ","
            << reviewQuery.value(5).toString() << ","
            << reviewQuery.value(6).toString() << "\n";
    }
    out << "\n";

    file.close();
    QMessageBox::information(this, tr("Success"), tr("All data exported successfully to %1").arg(fileName));
}

void MainWindow::showSummaryPopup(bool weekly)
{
    QDate startDate = weekly ? QDate::currentDate().addDays(-6) : QDate::currentDate();
    QDate endDate = QDate::currentDate();

    // Study time
    int totalMinutes = 0;
    if (weekly) {
        QMap<QDate, int> history = studyStreak->getStudyHistory(startDate, endDate);
        for (auto minutes : history.values()) totalMinutes += minutes;
    } else {
        totalMinutes = studyStreak->getStudyHistory(endDate, endDate).value(endDate, 0);
    }

    // Sessions
    int sessionCount = 0;
    if (weekly) {
        for (int i = 0; i < 7; ++i) {
            QDate d = startDate.addDays(i);
            sessionCount += studySession->getSessionsForDate(d).size();
        }
    } else {
        sessionCount = studySession->getSessionsForDate(endDate).size();
    }

    // Goals
    QList<GoalInfo> goals = studyGoal->getGoalsForDate(endDate);
    int completedGoals = 0;
    for (const auto& goal : goals) {
        if (goal.completedMinutes >= goal.targetMinutes && goal.targetMinutes > 0)
            completedGoals++;
    }

    // Achievements
    int unlocked = achievementTracker->getUnlockedCount();
    int totalAchievements = achievementTracker->getTotalAchievements();

    // Streak
    int streak = studyStreak->getCurrentStreak();

    // Message
    QString summary = QString(
        "<b>%1 Summary</b><br><br>"
        "Total Study Time: <b>%2</b> minutes<br>"
        "Sessions Completed: <b>%3</b><br>"
        "Goals Completed: <b>%4/%5</b><br>"
        "Current Streak: <b>%6</b> days<br>"
        "Achievements Unlocked: <b>%7/%8</b><br><br>"
        "%9"
    ).arg(weekly ? "Weekly" : "Daily")
     .arg(totalMinutes)
     .arg(sessionCount)
     .arg(completedGoals)
     .arg(goals.size())
     .arg(streak)
     .arg(unlocked)
     .arg(totalAchievements)
     .arg(streak > 0 ? "Keep up the great work!" : "Let's get started!");

    QMessageBox msgBox;
    msgBox.setWindowTitle(weekly ? "Weekly Summary" : "Daily Summary");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(summary);
    msgBox.exec();
}

void MainWindow::updateGoalProgressBars()
{
    QList<GoalInfo> goals = studyGoal->getGoalsForDate(QDate::currentDate());
    for (int i = 0; i < goals.size(); ++i) {
        const GoalInfo& goal = goals[i];
        QLayoutItem* item = goalsCardLayout->itemAtPosition(i / 3, i % 3);
        if (!item) continue;
        QWidget* card = item->widget();
        if (!card) continue;
        QProgressBar* progress = card->findChild<QProgressBar*>();
        if (!progress) continue;

        int completed = goal.completedMinutes;
        int target = goal.targetMinutes > 0 ? goal.targetMinutes : 1;
        int total = completed;

        // Only use session progress for the currently tracked goal, and only if tracking is active
        if (goal.id == m_currentActiveGoalId && pomodoroTimer && pomodoroTimer->isRunning()) {
            int elapsedSeconds = m_currentSessionProgress.value(goal.id, 0);
            int elapsedMinutes = elapsedSeconds / 60;
            total = completed + elapsedMinutes;
        }
        // For all other goals, always use completedMinutes from the database

        float percent = (target > 0) ? (float)total / target * 100.0f : 0.0f;
        progress->setRange(0, target);
        progress->setValue(total);
        progress->setFormat(QString("%1/%2 minutes (%3%)").arg(total).arg(target).arg(percent, 0, 'f', 1));
    }
}

void MainWindow::showYearlyActivityDialog()
{
    int currentYear = QDate::currentDate().year();
    QDialog dialog(this);
    dialog.setWindowTitle("Yearly Activity");
    dialog.resize(1100, 340);
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // Year navigation controls
    QHBoxLayout *yearNavLayout = new QHBoxLayout();
    QPushButton *prevYearBtn = new QPushButton("◀");
    QPushButton *nextYearBtn = new QPushButton("▶");
    QLabel *yearLabel = new QLabel(QString::number(currentYear));
    yearLabel->setAlignment(Qt::AlignCenter);
    yearLabel->setMinimumWidth(60);
    yearNavLayout->addWidget(prevYearBtn);
    yearNavLayout->addWidget(yearLabel);
    yearNavLayout->addWidget(nextYearBtn);
    yearNavLayout->addStretch();
    mainLayout->addLayout(yearNavLayout);

    // Widgets to update
    QLabel *counter = new QLabel();
    mainLayout->addWidget(counter, 0, Qt::AlignLeft);
    QHBoxLayout *monthLayout = new QHBoxLayout();
    monthLayout->addSpacing(40);
    for (int m = 1; m <= 12; ++m) {
        QLabel *monthLabel = new QLabel(QLocale::system().monthName(m, QLocale::ShortFormat));
        monthLabel->setAlignment(Qt::AlignCenter);
        monthLabel->setMinimumWidth(70);
        monthLayout->addWidget(monthLabel);
    }
    mainLayout->addLayout(monthLayout);
    QGridLayout *grid = new QGridLayout();
    QStringList weekdays = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (int i = 0; i < weekdays.size(); ++i) {
        QLabel *lbl = new QLabel(weekdays[i]);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl, i+1, 0);
    }
    mainLayout->addLayout(grid);

    auto updateCalendar = [&](int year) {
        yearLabel->setText(QString::number(year));
        // Remove old cells
        QLayoutItem *item;
        while ((item = grid->takeAt(grid->count()-1))) {
            if (item->widget() && item->widget()->parent() == &dialog) {
                delete item->widget();
            }
            delete item;
        }
        // Re-add weekday labels
        for (int i = 0; i < weekdays.size(); ++i) {
            QLabel *lbl = new QLabel(weekdays[i]);
            lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            grid->addWidget(lbl, i+1, 0);
        }
        // Get study days
        QMap<QDate, int> studyDays = studyStreak->getStudyHistory(QDate(year, 1, 1), QDate(year, 12, 31));
        int totalEvents = 0;
        for (auto v : studyDays.values()) if (v > 0) totalEvents++;
        counter->setText(QString("<b>Total study days this year:</b> %1").arg(totalEvents));
        // Fill grid
        QDate firstDay(year, 1, 1);
        QDate lastDay(year, 12, 31);
        int colOffset = 1;
        QDate d = firstDay;
        while (d <= lastDay) {
            int week = d.weekNumber();
            int weekday = d.dayOfWeek();
            int col = ((d.dayOfYear() - 1) / 7) + colOffset;
            int row = weekday;
            QFrame *cell = new QFrame();
            cell->setFixedSize(16, 16);
            int minutes = studyDays.value(d, 0);
            // Heatmap color: white (no study), light red (few min), dark red (many min)
            QString color;
            if (minutes == 0) color = "#eee";
            else if (minutes < 15) color = "#ffb3b3";
            else if (minutes < 30) color = "#ff6666";
            else if (minutes < 60) color = "#ff1a1a";
            else if (minutes < 120) color = "#c3073f";
            else color = "#800000";
            cell->setStyleSheet(QString("background:%1; border-radius:3px; border:1px solid #eee;").arg(color));
            // Tooltip with date and minutes
            cell->setToolTip(QString("%1\n%2 minutes studied").arg(d.toString("yyyy-MM-dd")).arg(minutes));
            grid->addWidget(cell, row, col);
            d = d.addDays(1);
        }
    };
    int shownYear = currentYear;
    updateCalendar(shownYear);
    QObject::connect(prevYearBtn, &QPushButton::clicked, [&]() {
        shownYear--;
        updateCalendar(shownYear);
    });
    QObject::connect(nextYearBtn, &QPushButton::clicked, [&]() {
        shownYear++;
        updateCalendar(shownYear);
    });
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    dialog.exec();
}

// Add eventFilter to MainWindow to handle card clicks
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *card = qobject_cast<QWidget *>(obj);
        if (card && card->property("goalId").isValid()) {
            int goalId = card->property("goalId").toInt();
            // Find the goal info
            QList<GoalInfo> goals = studyGoal->getGoalsForDate(QDate::currentDate());
            for (const auto &goal : goals) {
                if (goal.id == goalId) {
                    goalSubjectInput->setText(goal.subject);
                    goalTargetMinutesInput->setValue(goal.targetMinutes);
                    goalNotesInput->setPlainText(goal.notes);
                    goalCategoryCombo->setCurrentText(goal.category);
                    recurrenceTypeCombo->setCurrentText(goal.recurrenceType);
                    recurrenceValueInput->setText(goal.recurrenceValue);
                    // Resources
                    QListWidget *resourceList = createGoalGroup->findChild<QListWidget *>();
                    if (resourceList) {
                        resourceList->clear();
                        for (const QString &res : goal.resources) {
                            resourceList->addItem(res);
                        }
                    }
                    m_currentSelectedGoalId = goal.id;
                    addGoalButton->setText("Update Goal");
                    cancelEditButton->setEnabled(true);
                    break;
                }
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::createSurveysTab()
{
    QWidget *surveysTab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(surveysTab);
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    QWidget *formWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(formWidget);
    Survey *survey = new Survey(db, this);

    // 1. Goal selection dropdown
    QComboBox *goalCombo = new QComboBox();
    QList<GoalInfo> completedGoals = studyGoal->getGoalsForDate(QDate::currentDate());
    QMap<int, QString> goalIdToText;
    for (const auto &goal : completedGoals) {
        if (goal.completedMinutes >= goal.targetMinutes && goal.targetMinutes > 0) {
            QString text = QString("%1 (%2)").arg(goal.subject, goal.category);
            goalCombo->addItem(text, goal.id);
            goalIdToText[goal.id] = text;
        }
    }
    layout->addWidget(new QLabel("Select Completed Goal:"));
    layout->addWidget(goalCombo);

    // 2. Mood/energy emoji grid
    layout->addWidget(new QLabel("How did you feel after this session?"));
    QWidget *emojiWidget = new QWidget();
    QGridLayout *emojiGrid = new QGridLayout(emojiWidget);
    struct EmojiMood { QString emoji; QString text; };
    QList<EmojiMood> emojiMoods = {
        {"😃", "Happy"}, {"🙂", "Content"}, {"😐", "Neutral"}, {"😫", "Tired"}, {"😡", "Frustrated"},
        {"😍", "Loved it"}, {"😎", "Confident"}, {"😭", "Overwhelmed"}, {"😴", "Sleepy"}, {"🤔", "Thoughtful"},
        {"😤", "Determined"}, {"😅", "Relieved"}, {"😇", "Proud"}, {"😬", "Anxious"}, {"😱", "Stressed"}
    };
    QList<QPushButton*> emojiButtons;
    int colCount = 5;
    for (int i = 0; i < emojiMoods.size(); ++i) {
        const EmojiMood &em = emojiMoods[i];
        QPushButton *btn = new QPushButton(em.emoji + " " + em.text);
        btn->setCheckable(true);
        btn->setStyleSheet("font-size: 20px; background: transparent; border: 2px solid transparent; border-radius: 8px; padding: 4px 10px; transition: background 0.2s, color 0.2s, border 0.2s;");
        emojiGrid->addWidget(btn, i / colCount, i % colCount);
        emojiButtons.append(btn);
        connect(btn, &QPushButton::clicked, this, [=]() {
            if (btn->isChecked()) {
                btn->setStyleSheet("font-size: 20px; background: #950740; color: white; border: 2px solid #950740; border-radius: 8px; padding: 4px 10px; transition: background 0.2s, color 0.2s, border 0.2s;");
            } else {
                btn->setStyleSheet("font-size: 20px; background: transparent; border: 2px solid transparent; border-radius: 8px; padding: 4px 10px; transition: background 0.2s, color 0.2s, border 0.2s;");
            }
        });
    }
    layout->addWidget(emojiWidget);

    // 3. Distraction level slider
    layout->addWidget(new QLabel("Distraction Level (0 = none, 100 = max):"));
    QSlider *distractionSlider = new QSlider(Qt::Horizontal);
    distractionSlider->setRange(0, 100);
    distractionSlider->setValue(0);
    distractionSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 8px; background: #C3073F; border-radius: 4px; }"
        "QSlider::handle:horizontal { background: #950740; border: 2px solid #C3073F; width: 18px; height: 18px; border-radius: 9px; margin: -5px 0; }"
        "QSlider::sub-page:horizontal { background: #C3073F; border-radius: 4px; }"
        "QSlider::add-page:horizontal { background: #333; border-radius: 4px; }"
    );
    layout->addWidget(distractionSlider);
    QLabel *sliderValueLabel = new QLabel("0");
    sliderValueLabel->setAlignment(Qt::AlignCenter);
    sliderValueLabel->setStyleSheet("font-size: 14px; margin-top: 0px; padding-top: 0px; min-height: 0px; max-height: 18px;");
    layout->addWidget(sliderValueLabel);
    connect(distractionSlider, &QSlider::valueChanged, sliderValueLabel, [=](int value) {
        sliderValueLabel->setText(QString::number(value));
    });

    // 4. Distraction checklist
    layout->addWidget(new QLabel("What distracted you? (Select all that apply)"));
    QList<QCheckBox*> distractionChecks;
    for (const QString &d : survey->getCommonDistractions()) {
        QCheckBox *cb = new QCheckBox(d);
        layout->addWidget(cb);
        distractionChecks.append(cb);
    }

    // --- Session Satisfaction (Stars) ---
    layout->addWidget(new QLabel("Session Satisfaction:"));
    QHBoxLayout *starLayout = new QHBoxLayout();
    surveyStarButtons.clear();
int maxStars = 5;
for (int i = 0; i < maxStars; ++i) {
    QPushButton *starBtn = new QPushButton("☆");
    starBtn->setCheckable(true);
    starBtn->setStyleSheet("font-size: 28px; color: #C3073F; background: transparent; border: none;");
    starLayout->addWidget(starBtn);
    surveyStarButtons.append(starBtn);
}
for (int i = 0; i < maxStars; ++i) {
    connect(surveyStarButtons[i], &QPushButton::clicked, this, [this, i, maxStars]() {
        for (int j = 0; j < maxStars; ++j) {
            if (j <= i) {
                surveyStarButtons[j]->setText("★");
                surveyStarButtons[j]->setChecked(true);
            } else {
                surveyStarButtons[j]->setText("☆");
                surveyStarButtons[j]->setChecked(false);
            }
        }
    });
}
    starLayout->addStretch();
    layout->addLayout(starLayout);

    // --- Goal Achieved (Yes/No/Partial) ---
    layout->addWidget(new QLabel("Did you achieve your goal for this session?"));
    QHBoxLayout *goalAchievedLayout = new QHBoxLayout();
    QButtonGroup *goalAchievedGroup = new QButtonGroup(this);
    QRadioButton *yesBtn = new QRadioButton("Yes");
    QRadioButton *noBtn = new QRadioButton("No");
    QRadioButton *partialBtn = new QRadioButton("Partial");
    goalAchievedGroup->addButton(yesBtn);
    goalAchievedGroup->addButton(noBtn);
    goalAchievedGroup->addButton(partialBtn);
    goalAchievedLayout->addWidget(yesBtn);
    goalAchievedLayout->addWidget(noBtn);
    goalAchievedLayout->addWidget(partialBtn);
    goalAchievedLayout->addStretch();
    layout->addLayout(goalAchievedLayout);

    // --- Open Feedback (Text Box) ---
    layout->addWidget(new QLabel("Additional Feedback / Notes:"));
    QTextEdit *feedbackEdit = new QTextEdit();
    feedbackEdit->setPlaceholderText("Write any thoughts, challenges, or ideas...");
    feedbackEdit->setMinimumHeight(40);
    layout->addWidget(feedbackEdit);

    // --- Set Reminder (Yes/No) ---
    QCheckBox *reminderCheck = new QCheckBox("Set a reminder for your next session");
    layout->addWidget(reminderCheck);

    // 5. Save button
    QPushButton *saveBtn = new QPushButton("Save Survey");
    layout->addWidget(saveBtn);
    QLabel *statusLabel = new QLabel();
    layout->addWidget(statusLabel);

    connect(saveBtn, &QPushButton::clicked, this, [=]() {
        if (goalCombo->currentIndex() < 0) {
            statusLabel->setText("Please select a completed goal.");
            return;
        }
        int goalId = goalCombo->currentData().toInt();
        QStringList moods;
        for (QPushButton *btn : emojiButtons) {
            if (btn->isChecked()) {
                moods << btn->text();
            }
        }
        if (moods.isEmpty()) {
            statusLabel->setText("Please select your mood/energy.");
            return;
        }
        int distractionLevel = distractionSlider->value();
        QList<QString> distractions;
        for (QCheckBox *cb : distractionChecks) {
            if (cb->isChecked()) distractions.append(cb->text());
        }
        // --- Collect new fields ---
        int sessionSatisfaction = 0;
        for (int i = maxStars; i >= 1; --i) {
            if ( surveyStarButtons[i-1]->isChecked()) {
                sessionSatisfaction = i;
                break;
            }
        }
        QString goalAchieved;
        if (yesBtn->isChecked()) goalAchieved = "Yes";
        else if (noBtn->isChecked()) goalAchieved = "No";
        else if (partialBtn->isChecked()) goalAchieved = "Partial";
        QString openFeedback = feedbackEdit->toPlainText();
        bool setReminder = reminderCheck->isChecked();
        SurveyResult result;
        result.goalId = goalId;
        result.timestamp = QDateTime::currentDateTime();
        result.moodEmoji = moods.join(", ");
        result.distractionLevel = distractionLevel;
        result.distractions = distractions;
        result.sessionSatisfaction = sessionSatisfaction;
        result.goalAchieved = goalAchieved;
        result.openFeedback = openFeedback;
        result.setReminder = setReminder;
        if (survey->saveSurveyResult(result)) {
            statusLabel->setText("Survey saved! Thank you for your feedback.");
            for (QPushButton *btn : emojiButtons) btn->setChecked(false);
            distractionSlider->setValue(0);
            for (QCheckBox *cb : distractionChecks) cb->setChecked(false);
            for (QPushButton *btn :  surveyStarButtons) { btn->setChecked(false); btn->setText("☆"); }
            yesBtn->setChecked(false); noBtn->setChecked(false); partialBtn->setChecked(false);
            feedbackEdit->clear();
            reminderCheck->setChecked(false);
            int idx = tabWidget->indexOf(surveysTab);
             if (idx != -1) {
             tabWidget->removeTab(idx);
            }
            createSurveysTab();
            tabWidget->setCurrentIndex(tabWidget->count() - 1); // Optionally, keep user on Surveys tab
        } else {
            statusLabel->setText("Failed to save survey.");
        }
    });

    // --- Survey History Section ---
layout->addWidget(new QLabel("Survey History:"));
QScrollArea *historyScroll = new QScrollArea();
historyScroll->setWidgetResizable(true);
QWidget *historyWidget = new QWidget();
QHBoxLayout *historyLayout = new QHBoxLayout(historyWidget);
historyLayout->setSpacing(24); // More space between cards

// Get all goals with survey entries
QSet<int> goalIdsWithSurveys;
QList<SurveyResult> allResults = survey->getAllSurveyResults();
for (const SurveyResult &r : allResults) goalIdsWithSurveys.insert(r.goalId);

for (int goalId : goalIdsWithSurveys) {
    QString goalText = goalIdToText.value(goalId, QString("Goal #%1").arg(goalId));
    QPushButton *cardBtn = new QPushButton(goalText);
    cardBtn->setStyleSheet("background: #222; border: 1px solid #6F2232; border-radius: 8px; padding: 10px; font-weight: bold; font-size: 16px; text-align: left;");
    cardBtn->setCursor(Qt::PointingHandCursor);
    cardBtn->setFixedWidth(200);
    cardBtn->setFixedHeight(200);
    cardBtn->setMinimumHeight(80);
    historyLayout->addWidget(cardBtn);

    connect(cardBtn, &QPushButton::clicked, this, [=]() {
        // Show dialog with all survey entries for this goal
        QList<SurveyResult> results = survey->getSurveyResultsForGoal(goalId);
        QDialog dlg(this);
        dlg.setWindowTitle("Survey Entries for " + goalText);
        dlg.resize(600, 500);
        QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
        QListWidget *entryList = new QListWidget();
        for (const SurveyResult &res : results) {
            QString summary = res.timestamp.toString("yyyy-MM-dd hh:mm") + " | Mood: " + res.moodEmoji + " | Satisfaction: " + QString::number(res.sessionSatisfaction) + "★";
            QListWidgetItem *item = new QListWidgetItem(summary);
            item->setData(Qt::UserRole, res.timestamp.toString(Qt::ISODate));
            entryList->addItem(item);
        }
        dlgLayout->addWidget(entryList);
        QPushButton *editBtn = new QPushButton("Edit Selected Entry");
        QPushButton *deleteBtn = new QPushButton("Delete Selected Entry");
        QPushButton *exportBtn = new QPushButton("Export All Entries");
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addWidget(editBtn);
        btnLayout->addWidget(deleteBtn);
        btnLayout->addWidget(exportBtn);
        dlgLayout->addLayout(btnLayout);

        connect(editBtn, &QPushButton::clicked, this, [this, entryList, survey, goalId, cardBtn, historyLayout, historyWidget, &dlg, &results]() {
            QListWidgetItem *item = entryList->currentItem();
            if (!item) return;
            QString ts = item->data(Qt::UserRole).toString();
            // Find the entry
            SurveyResult res;
            for (const SurveyResult &r : results) {
                if (r.timestamp.toString(Qt::ISODate) == ts) { res = r; break; }
            }
            // Show edit dialog
            QDialog editDlg(&dlg);
            editDlg.setWindowTitle("Edit Survey Entry");
            QVBoxLayout *editLayout = new QVBoxLayout(&editDlg);
            // Mood
            QLineEdit *moodEdit = new QLineEdit(res.moodEmoji);
            editLayout->addWidget(new QLabel("Mood(s):"));
            editLayout->addWidget(moodEdit);
            // Distraction Level
            QSlider *distrSlider = new QSlider(Qt::Horizontal);
            distrSlider->setRange(0, 100);
            distrSlider->setValue(res.distractionLevel);
            editLayout->addWidget(new QLabel("Distraction Level:"));
            editLayout->addWidget(distrSlider);
            // Distractions
            QLineEdit *distrEdit = new QLineEdit(res.distractions.join(", "));
            editLayout->addWidget(new QLabel("Distractions (comma separated):"));
            editLayout->addWidget(distrEdit);
            // Satisfaction
            QSpinBox *satisSpin = new QSpinBox();
            satisSpin->setRange(1, 5);
            satisSpin->setValue(res.sessionSatisfaction);
            editLayout->addWidget(new QLabel("Session Satisfaction (stars):"));
            editLayout->addWidget(satisSpin);
            // Goal Achieved
            QComboBox *achCombo = new QComboBox();
            achCombo->addItems({"Yes", "No", "Partial"});
            achCombo->setCurrentText(res.goalAchieved);
            editLayout->addWidget(new QLabel("Goal Achieved:"));
            editLayout->addWidget(achCombo);
            // Feedback
            QTextEdit *fbEdit = new QTextEdit(res.openFeedback);
            editLayout->addWidget(new QLabel("Feedback/Notes:"));
            editLayout->addWidget(fbEdit);
            // Reminder
            QCheckBox *reminderBox = new QCheckBox("Set Reminder");
            reminderBox->setChecked(res.setReminder);
            editLayout->addWidget(reminderBox);
            QPushButton *saveEditBtn = new QPushButton("Save Changes");
            editLayout->addWidget(saveEditBtn);
            connect(saveEditBtn, &QPushButton::clicked, this, [this, &editDlg, survey, &res, moodEdit, distrSlider, distrEdit, satisSpin, achCombo, fbEdit, reminderBox, entryList]() {
                SurveyResult updated = res;
                updated.moodEmoji = moodEdit->text();
                updated.distractionLevel = distrSlider->value();
                updated.distractions = distrEdit->text().split(", ", Qt::SkipEmptyParts);
                updated.sessionSatisfaction = satisSpin->value();
                updated.goalAchieved = achCombo->currentText();
                updated.openFeedback = fbEdit->toPlainText();
                updated.setReminder = reminderBox->isChecked();
                survey->deleteSurveyResult(updated.goalId, updated.timestamp);
                survey->saveSurveyResult(updated);
                editDlg.accept();
                // Refresh the entry list in the dialog
                entryList->clear();
                QList<SurveyResult> refreshed = survey->getSurveyResultsForGoal(updated.goalId);
                for (const SurveyResult &r : refreshed) {
                    QString summary = r.timestamp.toString("yyyy-MM-dd hh:mm") + " | Mood: " + r.moodEmoji + " | Satisfaction: " + QString::number(r.sessionSatisfaction) + "★";
                    QListWidgetItem *item = new QListWidgetItem(summary);
                    item->setData(Qt::UserRole, r.timestamp.toString(Qt::ISODate));
                    entryList->addItem(item);
                }
            });
            editDlg.exec();
        });
        connect(deleteBtn, &QPushButton::clicked, this, [this, entryList, survey, goalId, cardBtn, historyLayout, historyWidget, &dlg, &results]() {
            QListWidgetItem *item = entryList->currentItem();
            if (!item) return;
            QString ts = item->data(Qt::UserRole).toString();
            // Find the entry
            SurveyResult res;
            for (const SurveyResult &r : results) {
                if (r.timestamp.toString(Qt::ISODate) == ts) { res = r; break; }
            }
            if (QMessageBox::question(this, "Delete Entry", "Are you sure you want to delete this survey entry?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                survey->deleteSurveyResult(res.goalId, res.timestamp);
                // Remove from UI
                delete item;
                // If no more entries, remove the goal card from history
                if (entryList->count() == 0) {
                    historyLayout->removeWidget(cardBtn);
                    cardBtn->deleteLater();
                    dlg.accept(); // Close the dialog
                }
            }
        });
        connect(exportBtn, &QPushButton::clicked, this, [this, &results, goalText]() {
            QString fileName = QFileDialog::getSaveFileName(this, "Export Survey Entries", goalText + ".csv", "CSV Files (*.csv)");
            if (fileName.isEmpty()) return;
            QFile file(fileName);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::critical(this, "Error", "Could not open file for writing.");
                return;
            }
            QTextStream out(&file);
            out << "Timestamp,Mood,Distraction Level,Distractions,Session Satisfaction,Goal Achieved,Feedback,Set Reminder\n";
            for (const SurveyResult &res : results) {
                QString mood = res.moodEmoji;
                mood.replace('"', "''");
                QString distractions = res.distractions.join("; ");
                distractions.replace('"', "''");
                QString achieved = res.goalAchieved;
                achieved.replace('"', "''");
                QString feedback = res.openFeedback;
                feedback.replace('"', "''");
                out << res.timestamp.toString(Qt::ISODate) << ","
                    << '"' << mood << '"' << ","
                    << res.distractionLevel << ","
                    << '"' << distractions << '"' << ","
                    << res.sessionSatisfaction << ","
                    << '"' << achieved << '"' << ","
                    << '"' << feedback << '"' << ","
                    << (res.setReminder ? "Yes" : "No") << "\n";
            }
            file.close();
            QMessageBox::information(this, "Export Complete", "Survey entries exported to " + fileName);
        });
        dlg.exec();
    });
}
historyLayout->addStretch();
historyScroll->setWidget(historyWidget);
historyScroll->setFixedHeight(300);
layout->addWidget(historyScroll);
    scrollArea->setWidget(formWidget);
    mainLayout->addWidget(scrollArea);
    tabWidget->addTab(surveysTab, "Surveys");
}

void MainWindow::openResourceInApp(const QString &res) {
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Resource Viewer");
    dlg->resize(900, 700);
    QVBoxLayout *layout = new QVBoxLayout(dlg);

    if (res.startsWith("http", Qt::CaseInsensitive)) {
        // Use WebEngine for web links
        QWebEngineView *view = new QWebEngineView(dlg);
        view->setUrl(QUrl(res));
        layout->addWidget(view);
    } else if (res.endsWith(".pdf", Qt::CaseInsensitive)) {
        // Use QPdfView for local PDFs
        QPdfDocument *pdfDoc = new QPdfDocument(dlg);
        if (pdfDoc->load(res) == QPdfDocument::Error::None) {
            QPdfView *pdfView = new QPdfView(dlg);
            pdfView->setDocument(pdfDoc);
            layout->addWidget(pdfView);
        } else {
            QMessageBox::warning(this, "PDF Error", "Failed to load PDF file.");
            delete pdfDoc;
            dlg->deleteLater();
            return;
        }
    } else {
        QMessageBox::information(this, "Open Resource", "This file type is not supported for in-app viewing. It will be opened externally.");
        QDesktopServices::openUrl(QUrl::fromLocalFile(res));
        dlg->deleteLater();
        return;
    }
    dlg->exec();
}

void MainWindow::createBuddyChatTab() {
    QWidget *buddyTab = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(buddyTab);

    // Splitter for chat (left) and conversation list (right)
    QSplitter *splitter = new QSplitter(Qt::Horizontal, buddyTab);
    QWidget *chatWidget = new QWidget();
    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);
    buddyChatList = new QListWidget(chatWidget);
    buddyChatList->setSelectionMode(QAbstractItemView::NoSelection);
    buddyChatList->setFocusPolicy(Qt::NoFocus);
    buddyChatList->setStyleSheet("QListWidget { background: #222; color: #fff; border: none; } QListWidget::item { border-radius: 8px; margin: 4px; padding: 8px; }");
    chatLayout->addWidget(buddyChatList, 1);
    QHBoxLayout *inputLayout = new QHBoxLayout();
    buddyAttachButton = new QPushButton(QIcon::fromTheme("document-open", QIcon(":/icons/app_icon.svg")), "", chatWidget);
    buddyAttachButton->setToolTip("Attach image");
    buddySpeakerButton = new QPushButton(QIcon::fromTheme("media-playback-start", QIcon()), "", chatWidget);
    buddySpeakerButton->setToolTip("Read aloud");
    buddyChatInput = new QLineEdit(chatWidget);
    buddyChatInput->setPlaceholderText("Type your message...");
    buddyChatSendButton = new QPushButton("Send", chatWidget);
    inputLayout->addWidget(buddyAttachButton);
    inputLayout->addWidget(buddySpeakerButton);
    inputLayout->addWidget(buddyChatInput, 1);
    inputLayout->addWidget(buddyChatSendButton);
    chatLayout->addLayout(inputLayout);
    // Prompt suggestions row
    QHBoxLayout *promptLayout = new QHBoxLayout();
    QStringList prompts = {"Summarize my notes", "Quiz me", "Give me a study plan", "Motivate me", "Explain this concept"};
    buddyPromptButtons.clear();
    for (const QString& prompt : prompts) {
        QPushButton* btn = new QPushButton(prompt, chatWidget);
        btn->setStyleSheet("background: #444; color: rgb(230, 223, 186); border-radius: 8px; padding: 4px 10px; font-weight: bold;");
        connect(btn, &QPushButton::clicked, this, [this, prompt]() {
            buddyChatInput->setText(prompt);
            buddyChatInput->setFocus();
        });
        promptLayout->addWidget(btn);
        buddyPromptButtons.append(btn);
    }
    chatLayout->addLayout(promptLayout);
    chatWidget->setLayout(chatLayout);

    // Conversation list panel
    QWidget *convWidget = new QWidget();
    QVBoxLayout *convLayout = new QVBoxLayout(convWidget);
    // Search bar
    buddyConversationSearch = new QLineEdit(convWidget);
    buddyConversationSearch->setPlaceholderText("Search conversations...");
    convLayout->addWidget(buddyConversationSearch);
    buddyNewConversationButton = new QPushButton("New Conversation", convWidget);
    buddyDeleteConversationButton = new QPushButton("Delete Conversation", convWidget);
    buddyConversationList = new QListWidget(convWidget);
    convLayout->addWidget(buddyNewConversationButton);
    convLayout->addWidget(buddyDeleteConversationButton);
    convLayout->addWidget(buddyConversationList, 1);
    convWidget->setLayout(convLayout);

    splitter->addWidget(chatWidget);
    splitter->addWidget(convWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);
    buddyTab->setLayout(mainLayout);
    tabWidget->addTab(buddyTab, "Buddy");

    // Load conversations from disk
    loadBuddyConversations();
    // Populate conversation list
    buddyConversationList->clear();
    for (const auto& conv : buddyConversations) {
        buddyConversationList->addItem(conv.title);
    }
    // If no conversations, start a new one
    if (buddyConversations.isEmpty()) {
        startNewBuddyConversation();
    } else {
        switchBuddyConversation(0);
    }

    connect(buddyChatSendButton, &QPushButton::clicked, this, &MainWindow::handleBuddyChatSend);
    connect(buddyChatInput, &QLineEdit::returnPressed, this, &MainWindow::handleBuddyChatSend);
    connect(buddyNewConversationButton, &QPushButton::clicked, this, &MainWindow::startNewBuddyConversation);
    connect(buddyDeleteConversationButton, &QPushButton::clicked, this, [this]() {
        if (currentConversationIndex >= 0 && currentConversationIndex < buddyConversations.size()) {
            deleteBuddyConversation(currentConversationIndex);
        }
    });
    connect(buddyConversationList, &QListWidget::currentRowChanged, this, &MainWindow::switchBuddyConversation);
    // Conversation renaming on double-click
    connect(buddyConversationList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        int idx = buddyConversationList->row(item);
        if (idx < 0 || idx >= buddyConversations.size()) return;
        bool ok = false;
        QString newTitle = QInputDialog::getText(this, "Rename Conversation", "New title:", QLineEdit::Normal, buddyConversations[idx].title, &ok);
        if (ok && !newTitle.trimmed().isEmpty()) {
            buddyConversations[idx].title = newTitle.trimmed();
            item->setText(newTitle.trimmed());
            saveBuddyConversations();
        }
    });
    // Conversation search
    connect(buddyConversationSearch, &QLineEdit::textChanged, this, &MainWindow::filterBuddyConversations);
    connect(buddyAttachButton, &QPushButton::clicked, this, &MainWindow::handleBuddyAttach);
    connect(buddySpeakerButton, &QPushButton::clicked, this, [this]() {
        // Read last Buddy message aloud
        if (currentConversationIndex >= 0 && currentConversationIndex < buddyConversations.size()) {
            for (int i = buddyConversations[currentConversationIndex].messages.size() - 1; i >= 0; --i) {
                if (buddyConversations[currentConversationIndex].messages[i].first == "model") {
                    handleBuddySpeaker(buddyConversations[currentConversationIndex].messages[i].second);
                    break;
                }
            }
        }
    });
}

void MainWindow::filterBuddyConversations(const QString& text) {
    buddyConversationSearchText = text.trimmed();
    buddyConversationList->clear();
    for (int i = 0; i < buddyConversations.size(); ++i) {
        const auto& conv = buddyConversations[i];
        bool match = conv.title.contains(buddyConversationSearchText, Qt::CaseInsensitive);
        if (!match) {
            for (const auto& msg : conv.messages) {
                if (msg.second.contains(buddyConversationSearchText, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        if (match) {
            buddyConversationList->addItem(conv.title);
        }
    }
    // Optionally, reset selection if current is filtered out
    if (buddyConversationList->count() > 0) {
        buddyConversationList->setCurrentRow(0);
    }
}

void MainWindow::loadBuddyConversations() {
    buddyConversations.clear();
    QFile file(QCoreApplication::applicationDirPath() + "/buddy_conversations.json");
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) return;
    QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        Conversation conv;
        conv.id = obj["id"].toString();
        conv.title = obj["title"].toString();
        conv.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        QJsonArray msgs = obj["messages"].toArray();
        for (const QJsonValue& mv : msgs) {
            QJsonObject mobj = mv.toObject();
            conv.messages.append({mobj["role"].toString(), mobj["text"].toString()});
        }
        buddyConversations.append(conv);
    }
}

void MainWindow::saveBuddyConversations() {
    QJsonArray arr;
    for (const auto& conv : buddyConversations) {
        QJsonObject obj;
        obj["id"] = conv.id;
        obj["title"] = conv.title;
        obj["timestamp"] = conv.timestamp.toString(Qt::ISODate);
        QJsonArray msgs;
        for (const auto& msg : conv.messages) {
            QJsonObject mobj;
            mobj["role"] = msg.first;
            mobj["text"] = msg.second;
            msgs.append(mobj);
        }
        obj["messages"] = msgs;
        arr.append(obj);
    }
    QFile file(QCoreApplication::applicationDirPath() + "/buddy_conversations.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
    }
}

void MainWindow::startNewBuddyConversation() {
    Conversation conv;
    conv.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    conv.title = "Conversation " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    conv.timestamp = QDateTime::currentDateTime();
    conv.messages.clear();
    conv.messages.append(QPair<QString, QString>("model", "Hi! I'm your Study Buddy. How can I help you today?"));
    buddyConversations.prepend(conv);
    currentConversationIndex = 0;
    // Update UI
    buddyConversationList->insertItem(0, conv.title);
    buddyConversationList->setCurrentRow(0);
    switchBuddyConversation(0);
    saveBuddyConversations();
}

// Helper to add a Markdown message to the chat
static void addMarkdownMessage(QListWidget* list, const QString& text, bool isBuddy) {
    QWidget* widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(widget);
    QTextEdit* label = new QTextEdit();
    label->setReadOnly(true);
    label->setMarkdown(text); 
    label->setStyleSheet(isBuddy ? "color:rgb(230, 223, 186); font-weight: bold;" : "color: #C5C6C7;");
    label->setFrameStyle(QFrame::NoFrame);
    label->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    label->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    label->setStyleSheet(isBuddy ? "color:rgb(230, 223, 186); font-weight: bold;" : "color: #C5C6C7;");
    layout->addWidget(label);
    layout->setContentsMargins(0,0,0,0);
    widget->setLayout(layout);
    QListWidgetItem* item = new QListWidgetItem(list);
    item->setSizeHint(widget->sizeHint());
    list->setItemWidget(item, widget);
}

void MainWindow::switchBuddyConversation(int index) {
    if (index < 0 || index >= buddyConversations.size()) return;
    currentConversationIndex = index;
    buddyChatList->clear();
    for (const auto& msg : buddyConversations[index].messages) {
        if (msg.first == "user")
            addMarkdownMessage(buddyChatList, "You: " + msg.second, false);
        else
            addMarkdownMessage(buddyChatList, "🧑‍🎓 Buddy: " + msg.second, true);
    }
}

void MainWindow::deleteBuddyConversation(int index) {
    if (index < 0 || index >= buddyConversations.size()) return;
    buddyConversations.removeAt(index);
    buddyConversationList->takeItem(index);
    if (buddyConversations.isEmpty()) {
        startNewBuddyConversation();
    } else {
        int newIndex = qMin(index, buddyConversations.size() - 1);
        buddyConversationList->setCurrentRow(newIndex);
        switchBuddyConversation(newIndex);
    }
    saveBuddyConversations();
}

void MainWindow::handleBuddyChatSend() {
    if (currentConversationIndex < 0 || currentConversationIndex >= buddyConversations.size()) return;
    QString userMsg = buddyChatInput->text().trimmed();
    if (userMsg.isEmpty() && buddyPendingImagePath.isEmpty()) return;
    QString displayMsg = userMsg;
    if (!buddyPendingImagePath.isEmpty()) displayMsg = "[Image attached] " + displayMsg;
    addMarkdownMessage(buddyChatList, "You: " + displayMsg, false);
    buddyChatInput->clear();

    // Add user message to current conversation
    buddyConversations[currentConversationIndex].messages.append(QPair<QString, QString>("user", displayMsg));
    while (buddyConversations[currentConversationIndex].messages.size() > 20) buddyConversations[currentConversationIndex].messages.removeFirst();
    saveBuddyConversations();

    // Prepare Gemini API request with context (NO system role)
    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + GEMINI_API_KEY);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Add system prompt as a user message at the start
    QJsonArray contents;
    contents.append(QJsonObject{
        {"role", "user"},
        {"parts", QJsonArray{ QJsonObject{{"text", "You are Study Buddy, a friendly, encouraging AI study assistant for students. Be supportive, concise, and motivational."}} }}
    });
    // Inject current goals as a user message
    QList<GoalInfo> buddyGoals = studyGoal->getGoalsForDate(QDate::currentDate());
    if (!buddyGoals.isEmpty()) {
        QStringList goalSummaries;
        for (const auto& g : buddyGoals) {
            goalSummaries << QString("- %1 (%2/%3 min, %4)").arg(g.subject).arg(g.completedMinutes).arg(g.targetMinutes).arg(g.category);
        }
        QString goalsText = QString("Here are my current study goals for today:\n%1").arg(goalSummaries.join("\n"));
        contents.append(QJsonObject{
            {"role", "user"},
            {"parts", QJsonArray{ QJsonObject{{"text", goalsText}} }}
        });
    }
    for (const auto& pair : buddyConversations[currentConversationIndex].messages) {
        QJsonObject msgObj;
        msgObj["role"] = pair.first;
        QJsonArray parts;
        // If this is the current user message and an image is attached, add both text and image parts
        if (pair.first == "user" && !buddyPendingImagePath.isEmpty() && pair.second == displayMsg) {
            if (!userMsg.isEmpty()) parts.append(QJsonObject{{"text", userMsg}});
            // Read image and encode as base64
            QImage img(buddyPendingImagePath);
            if (!img.isNull()) {
                QByteArray ba;
                QBuffer buf(&ba);
                img.save(&buf, "PNG");
                QString b64 = QString::fromLatin1(ba.toBase64());
                QJsonObject imgPart;
                imgPart["inlineData"] = QJsonObject{
                    {"mimeType", "image/png"},
                    {"data", b64}
                };
                parts.append(imgPart);
            }
        } else {
            parts.append(QJsonObject{{"text", pair.second}});
        }
        msgObj["parts"] = parts;
        contents.append(msgObj);
    }
    QJsonObject reqObj;
    reqObj["contents"] = contents;

    QJsonDocument doc(reqObj);
    QByteArray data = doc.toJson();

    // Add typing indicator
    QListWidgetItem* typingItem = new QListWidgetItem(buddyChatList);
    QWidget* typingWidget = new QWidget();
    QHBoxLayout* typingLayout = new QHBoxLayout(typingWidget);
    QLabel* typingLabel = new QLabel("🧑‍🎓 Buddy is typing...");
    typingLayout->addWidget(typingLabel);
    typingLayout->setContentsMargins(0,0,0,0);
    typingWidget->setLayout(typingLayout);
    typingItem->setSizeHint(typingWidget->sizeHint());
    buddyChatList->setItemWidget(typingItem, typingWidget);
    buddyChatList->scrollToBottom();

    // Send POST request
    QNetworkReply* reply = geminiNetworkManager->post(request, data);

    // Handle the reply asynchronously
    connect(reply, &QNetworkReply::finished, this, [this, reply, typingItem]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        // Remove typing indicator
        int row = buddyChatList->row(typingItem);
        if (row >= 0) buddyChatList->takeItem(row);
        delete typingItem;

        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QString buddyReply = "Sorry, I couldn't get a response from Gemini.";
        if (jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            if (obj.contains("candidates")) {
                QJsonArray candidates = obj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject cand = candidates[0].toObject();
                    if (cand.contains("content")) {
                        QJsonObject content = cand["content"].toObject();
                        if (content.contains("parts")) {
                            QJsonArray parts = content["parts"].toArray();
                            if (!parts.isEmpty()) {
                                QJsonObject part = parts[0].toObject();
                                buddyReply = part.value("text").toString();
                            }
                        }
                    }
                }
            } else if (obj.contains("error")) {
                buddyReply = "Gemini API error: " + obj["error"].toObject().value("message").toString();
            }
        }
        // Add Buddy reply to current conversation
        if (currentConversationIndex >= 0 && currentConversationIndex < buddyConversations.size()) {
            buddyConversations[currentConversationIndex].messages.append(QPair<QString, QString>("model", buddyReply));
            while (buddyConversations[currentConversationIndex].messages.size() > 20) buddyConversations[currentConversationIndex].messages.removeFirst();
            saveBuddyConversations();
        }
        addMarkdownMessage(buddyChatList, "🧑‍🎓 Buddy: " + buddyReply, true);
        buddyChatList->scrollToBottom();
        buddyPendingImagePath.clear();
        buddyChatInput->setPlaceholderText("Type your message...");
    });
}

void MainWindow::handleBuddyAttach() {
    QString file = QFileDialog::getOpenFileName(this, "Attach Image", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!file.isEmpty()) {
        buddyPendingImagePath = file;
        buddyChatInput->setPlaceholderText("Image attached. Type your message...");
    }
}

void MainWindow::handleBuddySpeaker(const QString &text) {
#ifdef Q_OS_WIN
    QAxObject voice("SAPI.SpVoice");
    voice.dynamicCall("Speak(const QString&)", text);
#else
    QMessageBox::information(this, "Text-to-Speech", "Text-to-speech is only supported on Windows with QtAxContainer.");
#endif
}

void MainWindow::notifyOverdueAndTodayGoals() {
    if (!trayIcon || !QSystemTrayIcon::supportsMessages()) return;
    QDate today = QDate::currentDate();
    // Query all active goals
    QList<GoalInfo> allGoals = studyGoal->getGoalsForDate(QDate(2000,1,1)); // Use a very early date to get all active goals
    QStringList overdue, dueToday;
    for (const auto& goal : allGoals) {
        QDate due = goal.dueDate.isEmpty() ? QDate() : QDate::fromString(goal.dueDate, Qt::ISODate);
        QDate start = goal.startDate.isEmpty() ? QDate() : QDate::fromString(goal.startDate, Qt::ISODate);
        if (goal.status != "Completed") {
            if (!due.isNull() && due < today) overdue << goal.subject;
            else if ((start == today) || (!due.isNull() && due == today)) dueToday << goal.subject;
        }
    }
    if (!overdue.isEmpty()) {
        trayIcon->showMessage(tr("Overdue Goals"), tr("You have overdue goals: %1").arg(overdue.join(", ")), QSystemTrayIcon::Warning, 5000);
    }
    if (!dueToday.isEmpty()) {
        trayIcon->showMessage(tr("Today's Goals"), tr("Goals due today: %1").arg(dueToday.join(", ")), QSystemTrayIcon::Information, 5000);
    }
}

void MainWindow::createCalendarTab() {
    QWidget *calendarTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(calendarTab);
    calendarWidget = new QCalendarWidget(calendarTab);
    calendarWidget->setGridVisible(true);
    calendarWidget->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    QTextCharFormat todayFormat;
    todayFormat.setForeground(Qt::red);
    calendarWidget->setDateTextFormat(QDate::currentDate(), todayFormat);
    calendarWidget->setStyleSheet(R"(
        QCalendarWidget QAbstractItemView {
            background-color: #1A1A1D;
            selection-background-color: #6F2232;
            selection-color: #fff;
            border: 1px solid  #6F2232;
            color: #C5C6C7;
            gridline-color: #6F2232;
        }
        QCalendarWidget QWidget#qt_calendar_navigationbar {
            background-color: #6F2232;
            color: #fff;
        }
        QCalendarWidget QToolButton {
            background-color: #6F2232;
            color: #fff;
            border-radius: 4px;
            margin: 2px;
            padding: 2px 8px;
        }
        QCalendarWidget QToolButton:hover {
            background-color: #950740;
        }
        QCalendarWidget QSpinBox {
            background: #222;
            color: #fff;
            border: 1px solid #6F2232;
        }
    )");
    layout->addWidget(calendarWidget);
    QPushButton *addGoalBtn = new QPushButton("+ Add Goal");
    layout->addWidget(addGoalBtn);
    calendarGoalsList = new QListWidget(calendarTab);
    layout->addWidget(calendarGoalsList);
    tabWidget->addTab(calendarTab, "Calendar");
    connect(calendarWidget, &QCalendarWidget::activated, this, [this](const QDate& date) {
        openDayPlannerDialog(date);
    });
    // Update goals list when date is selected
    auto refreshGoalsList = [this]() {
        QDate date = calendarWidget->selectedDate();
        calendarGoalsList->clear();
        QList<GoalInfo> goals = studyGoal->getGoalsForDate(date);
        QString dayName = date.toString("ddd");
        for (const auto& goal : goals) {
            if (goal.recurrenceType == "Weekly" && !goal.recurrenceValue.isEmpty()) {
                QStringList recurDays = goal.recurrenceValue.split(",", Qt::SkipEmptyParts);
                if (!recurDays.contains(dayName)) continue;
            }
            QString status = (goal.completedMinutes >= goal.targetMinutes && goal.targetMinutes > 0) ? "[Completed]" : "[Active]";
            QListWidgetItem *item = new QListWidgetItem(QString("%1 %2").arg(goal.subject, status));
            item->setData(Qt::UserRole, goal.id);
            calendarGoalsList->addItem(item);
        }
        if (calendarGoalsList->count() == 0) {
            calendarGoalsList->addItem("No goals for this date.");
        }
    };
    connect(calendarWidget, &QCalendarWidget::selectionChanged, this, refreshGoalsList);
    // Show today's goals by default
    calendarWidget->setSelectedDate(QDate::currentDate());
    emit calendarWidget->selectionChanged();
    // Add Goal button logic
    connect(addGoalBtn, &QPushButton::clicked, this, [this, refreshGoalsList]() {
        QDate date = calendarWidget->selectedDate();
        QDialog dlg(this);
        dlg.setWindowTitle("Add Goal");
        QFormLayout *form = new QFormLayout(&dlg);
        QLineEdit *subjectEdit = new QLineEdit();
        QSpinBox *minutesEdit = new QSpinBox(); minutesEdit->setRange(1, 1000);
        QTextEdit *notesEdit = new QTextEdit();
        QComboBox *recurrenceTypeCombo = new QComboBox();
        recurrenceTypeCombo->addItems({"None", "Daily", "Weekly", "Monthly"});
        QWidget *recurrenceValueWidget = new QWidget();
        QHBoxLayout *recurrenceValueLayout = new QHBoxLayout(recurrenceValueWidget);
        recurrenceValueLayout->setContentsMargins(0,0,0,0);
        // For weekly: checkboxes for days
        QList<QCheckBox*> weekDayChecks;
        QStringList weekDays = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        for (const QString& day : weekDays) {
            QCheckBox *cb = new QCheckBox(day);
            cb->setEnabled(false);
            recurrenceValueLayout->addWidget(cb);
            weekDayChecks.append(cb);
        }
        // Enable checkboxes only for Weekly
        connect(recurrenceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx){
            bool isWeekly = (recurrenceTypeCombo->currentText() == "Weekly");
            for (QCheckBox *cb : weekDayChecks) cb->setEnabled(isWeekly);
        });
        form->addRow("Subject:", subjectEdit);
        form->addRow("Target Minutes:", minutesEdit);
        form->addRow("Notes:", notesEdit);
        form->addRow("Recurrence:", recurrenceTypeCombo);
        form->addRow("Recurrence Details:", recurrenceValueWidget);
        QLabel *dateLabel = new QLabel(date.toString("yyyy-MM-dd"));
        form->addRow("Date:", dateLabel);
        QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(box);
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() == QDialog::Accepted) {
            QString recurrenceType = recurrenceTypeCombo->currentText();
            QString recurrenceValue;
            if (recurrenceType == "Weekly") {
                QStringList days;
                for (QCheckBox *cb : weekDayChecks) if (cb->isChecked()) days << cb->text();
                recurrenceValue = days.join(",");
            }
            studyGoal->createGoal(subjectEdit->text(), minutesEdit->value(), notesEdit->toPlainText(), recurrenceType, recurrenceValue, "Uncategorized", QStringList());
            refreshGoalsList();
        }
    });
    // Edit goal on double-click
    connect(calendarGoalsList, &QListWidget::itemDoubleClicked, this, [this, refreshGoalsList](QListWidgetItem *item) {
        if (!item->data(Qt::UserRole).isValid()) return;
        int goalId = item->data(Qt::UserRole).toInt();
        QMap<QString, QVariant> details = studyGoal->getGoalDetails(goalId);
        QDialog dlg(this);
        dlg.setWindowTitle("Edit Goal");
        QFormLayout *form = new QFormLayout(&dlg);
        QLineEdit *subjectEdit = new QLineEdit(details["subject"].toString());
        QSpinBox *minutesEdit = new QSpinBox(); minutesEdit->setRange(1, 1000); minutesEdit->setValue(details["target_minutes"].toInt());
        QTextEdit *notesEdit = new QTextEdit(details["notes"].toString());
        QComboBox *recurrenceTypeCombo = new QComboBox();
        recurrenceTypeCombo->addItems({"None", "Daily", "Weekly", "Monthly"});
        QWidget *recurrenceValueWidget = new QWidget();
        QHBoxLayout *recurrenceValueLayout = new QHBoxLayout(recurrenceValueWidget);
        recurrenceValueLayout->setContentsMargins(0,0,0,0);
        QList<QCheckBox*> weekDayChecks;
        QStringList weekDays = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        for (const QString& day : weekDays) {
            QCheckBox *cb = new QCheckBox(day);
            cb->setEnabled(false);
            recurrenceValueLayout->addWidget(cb);
            weekDayChecks.append(cb);
        }
        // Set initial recurrence type and value
        int idx = recurrenceTypeCombo->findText(details["recurrence_type"].toString());
        if (idx >= 0) recurrenceTypeCombo->setCurrentIndex(idx);
        if (details["recurrence_type"].toString() == "Weekly") {
            QStringList recurDays = details["recurrence_value"].toString().split(",", Qt::SkipEmptyParts);
            for (QCheckBox *cb : weekDayChecks) {
                cb->setEnabled(true);
                if (recurDays.contains(cb->text())) cb->setChecked(true);
            }
        }
        connect(recurrenceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx){
            bool isWeekly = (recurrenceTypeCombo->currentText() == "Weekly");
            for (QCheckBox *cb : weekDayChecks) cb->setEnabled(isWeekly);
        });
        form->addRow("Subject:", subjectEdit);
        form->addRow("Target Minutes:", minutesEdit);
        form->addRow("Notes:", notesEdit);
        form->addRow("Recurrence:", recurrenceTypeCombo);
        form->addRow("Recurrence Details:", recurrenceValueWidget);
        QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(box);
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() == QDialog::Accepted) {
            QString recurrenceType = recurrenceTypeCombo->currentText();
            QString recurrenceValue;
            if (recurrenceType == "Weekly") {
                QStringList days;
                for (QCheckBox *cb : weekDayChecks) if (cb->isChecked()) days << cb->text();
                recurrenceValue = days.join(",");
            }
            studyGoal->updateGoal(goalId, subjectEdit->text(), minutesEdit->value(), notesEdit->toPlainText(), recurrenceType, recurrenceValue, details["category"].toString(), QStringList());
            refreshGoalsList();
        }
    });
}
void MainWindow::openDayPlannerDialog(const QDate& date) {
    QDialog dlg(this);
    dlg.setWindowTitle(QString("Smart Planner for %1").arg(date.toString("yyyy-MM-dd")));
    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    struct Period { QString label; QString key; int start, end; };
    QVector<Period> periods = {
        {"🌞 Morning (06:00–12:00)", "morning", 6, 12},
        {"🌇 Evening (12:00–18:00)", "evening", 12, 18},
        {"🌙 Night (18:00–00:00)", "night", 18, 24}
    };
    QMap<QString, QSpinBox*> freeTimeInputs;
    QMap<QString, int> loaded = loadFreeTimeForDate(date);
    for (const auto& p : periods) {
        QGroupBox *box = new QGroupBox(p.label);
        QHBoxLayout *hl = new QHBoxLayout(box);
        hl->addWidget(new QLabel("Free time (min):"));
        QSpinBox *spin = new QSpinBox(); spin->setRange(0, 360); spin->setValue(loaded.value(p.key, 0));
        hl->addWidget(spin);
        freeTimeInputs[p.key] = spin;
        mainLayout->addWidget(box);
    }

    QPushButton *generateBtn = new QPushButton("Generate Plan");
    mainLayout->addWidget(generateBtn);
    QTextEdit *planOutput = new QTextEdit();
    planOutput->setReadOnly(true);
    mainLayout->addWidget(planOutput);

    // Save free time on change
    auto saveFreeTime = [=]() {
        saveFreeTimeForDate(date, freeTimeInputs["morning"]->value(), freeTimeInputs["evening"]->value(), freeTimeInputs["night"]->value());
    };
    for (auto spin : freeTimeInputs) connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), saveFreeTime);

    // Generate plan logic
    connect(generateBtn, &QPushButton::clicked, this, [=]() {
        saveFreeTime();
        int morningFree = freeTimeInputs["morning"]->value();
        int eveningFree = freeTimeInputs["evening"]->value();
        int nightFree = freeTimeInputs["night"]->value();
        QList<GoalInfo> goals = studyGoal->getGoalsForDate(date);
        QStringList plan;
        int m = morningFree, e = eveningFree, n = nightFree;
        for (const auto& goal : goals) {
            int minutes = goal.targetMinutes - goal.completedMinutes;
            if (minutes <= 0) continue;
            int remaining = minutes;
            QStringList assignedParts;
            if (m > 0 && remaining > 0) {
                int assign = qMin(m, remaining);
                assignedParts << QString("Morning: %1 min").arg(assign);
                m -= assign;
                remaining -= assign;
            }
            if (e > 0 && remaining > 0) {
                int assign = qMin(e, remaining);
                assignedParts << QString("Evening: %1 min").arg(assign);
                e -= assign;
                remaining -= assign;
            }
            if (n > 0 && remaining > 0) {
                int assign = qMin(n, remaining);
                assignedParts << QString("Night: %1 min").arg(assign);
                n -= assign;
                remaining -= assign;
            }
            if (assignedParts.isEmpty()) {
                plan << QString("%1: %2 min → Not enough free time").arg(goal.subject).arg(minutes);
            } else {
                plan << QString("%1: %2 min → %3%4")
                    .arg(goal.subject)
                    .arg(minutes)
                    .arg(assignedParts.join(", "))
                    .arg(remaining > 0 ? QString(", %1 min unscheduled").arg(remaining) : "");
            }
        }
        planOutput->setText(plan.join("\n"));
    });

    dlg.exec();
}

QMap<QString, int> MainWindow::loadFreeTimeForDate(const QDate& date) {
    QMap<QString, int> result;
    QSqlQuery q(db);
    q.prepare("SELECT morning_minutes, evening_minutes, night_minutes FROM free_time WHERE date = ?");
    q.addBindValue(date.toString(Qt::ISODate));
    if (q.exec() && q.next()) {
        result["morning"] = q.value(0).toInt();
        result["evening"] = q.value(1).toInt();
        result["night"] = q.value(2).toInt();
    } else {
        result["morning"] = 0;
        result["evening"] = 0;
        result["night"] = 0;
    }
    return result;
}
void MainWindow::saveFreeTimeForDate(const QDate& date, int morning, int evening, int night) {
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO free_time (date, morning_minutes, evening_minutes, night_minutes) VALUES (?, ?, ?, ?)");
    q.addBindValue(date.toString(Qt::ISODate));
    q.addBindValue(morning);
    q.addBindValue(evening);
    q.addBindValue(night);
    q.exec();
}

void MainWindow::createHelpTab()
{
    QWidget *helpTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(helpTab);
    QLabel *title = new QLabel("<h2>Help & FAQ</h2>");
    layout->addWidget(title);
    QTextEdit *faqText = new QTextEdit();
    faqText->setReadOnly(true);
    faqText->setHtml(R"(
<h3>How do I create a new goal?</h3>
<p>Go to the <b>Dashboard</b> tab and fill out the <b>Create New Goal</b> form at the top. Enter the subject, target minutes, notes, category, and (optionally) set recurrence. Click <b>Add Goal</b> to save it.</p>
<h3>How do I add resources to a goal?</h3>
<p>In the <b>Create New Goal</b> form, use the <b>Resources</b> section to paste a YouTube link, file path, or browse for a file. Click <b>Add Resource</b> to attach it to your goal. Double-click a resource to remove it.</p>
<h3>How do I track a goal?</h3>
<ul>
<li><b>With Pomodoro Timer:</b> On the Dashboard, click <b>Start</b> on your goal card. The Pomodoro timer will begin tracking your session. Pause or stop as needed.</li>
<li><b>With Camera Session:</b> Go to the <b>Webcam</b> tab, click <b>Start Session</b>, select your goal(s), and begin. The app will track your focus and session time using your webcam.</li>
</ul>
<h3>How do I edit or delete a goal?</h3>
<p>On the Dashboard, click a goal card to edit its details. Click <b>Delete</b> to remove a goal.</p>
<h3>How do I use the Calendar and Smart Planner?</h3>
<ul>
<li>Go to the <b>Calendar</b> tab to see your goals by date.</li>
<li>Click a day to open the Smart Planner. Enter your free time for morning, evening, and night, then click <b>Generate Plan</b> to organize your goals into your available time.</li>
<li>Add or edit goals directly from the calendar by clicking <b>+ Add Goal</b> or double-clicking a goal.</li>
</ul>
<h3>How do I use Achievements?</h3>
<p>Go to the <b>Achievements</b> tab to view milestones you can unlock. Achievements are earned for streaks, focus, session time, and more. Click an achievement to see its description and progress.</p>
<h3>How do I use Surveys?</h3>
<p>After completing a goal, go to the <b>Surveys</b> tab to reflect on your session. Fill out the mood, distractions, satisfaction, and feedback. Surveys help you identify patterns and improve your study habits.</p>
<h3>How do I use Analytics?</h3>
<ul>
<li>Use the <b>Analytics</b> tab to explore your study patterns, focus, and progress.</li>
<li>Hover over each chart for a tooltip explaining what it shows.</li>
<li>Use the date range selectors to filter your data.</li>
<li>Check the <b>Study Insights</b> section for smart tips and patterns detected by the app.</li>
</ul>
<h3>How do I get notifications?</h3>
<p>Enable notifications in the <b>Settings</b> tab. You can choose which events trigger notifications (Pomodoro completion, achievements, streaks, etc.).</p>
<h3>How do I export my data?</h3>
<p>Use the <b>Export</b> button in the Dashboard or Analytics tab to save your data as a CSV file for further analysis or sharing.</p>
<h3>Who can I contact for support?</h3>
<p>For support or feedback, email: <b>support@studybuddy.app</b> or use the feedback form in the app (if available).</p>
<h3>Tips</h3>
<ul>
<li>Hover over charts and buttons for tooltips.</li>
<li>Check the <b>Achievements</b> tab for milestones you can unlock!</li>
<li>Use the <b>Surveys</b> tab to reflect on your sessions and improve your habits.</li>
<li>Recurring goals can be set to repeat daily, weekly, or monthly.</li>
<li>Use the <b>Settings</b> tab to customize Pomodoro durations, streak thresholds, and notification preferences.</li>
<li>Missed a day? Check your streak and get back on track!</li>
</ul>
)");
    faqText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(faqText);
    QPushButton *walkthroughBtn = new QPushButton("Show Walkthrough");
    layout->addWidget(walkthroughBtn);
    connect(walkthroughBtn, &QPushButton::clicked, this, &MainWindow::startWalkthroughOverlay);
    tabWidget->addTab(helpTab, "Help");
}

void MainWindow::startWalkthroughOverlay() {
    if (walkthroughOverlay) {
        walkthroughOverlay->hide();
        walkthroughOverlay->deleteLater();
    }
    walkthroughOverlay = new WalkthroughOverlay(this);
    walkthroughOverlay->setGeometry(this->rect());
    QList<WalkthroughOverlay::Step> steps;
    // Helper to get tab rect by name
    auto tabRect = [this](const QString& name) -> QRect {
        QTabBar* bar = tabWidget->findChild<QTabBar*>();
        if (!bar) return QRect();
        const int highlightYOffset = 24; // Move highlight down
        for (int i = 0; i < bar->count(); ++i) {
            if (tabWidget->tabText(i).toLower() == name.toLower()) {
                QRect r = bar->tabRect(i);
                QPoint global = bar->mapTo(this, r.topLeft()) + QPoint(0, highlightYOffset);
                return QRect(global, r.size());
            }
        }
        return QRect();
    };
    steps.append({tabRect("Dashboard"), "This is the Dashboard tab. Here you can create, view, and track your study goals. Start Pomodoro sessions or edit goals here.", "Dashboard"});
    steps.append({tabRect("Webcam"), "The Webcam tab lets you track your focus using your camera. Start a session to monitor your attention and get real-time feedback.", "Webcam"});
    steps.append({tabRect("Buddy"), "The Buddy tab is your AI study assistant. Chat with Buddy for study tips, summaries, and motivation.", "Buddy"});
    steps.append({tabRect("Achievements"), "The Achievements tab shows your unlocked milestones and progress. Click an achievement to see details.", "Achievements"});
    steps.append({tabRect("Surveys"), "The Surveys tab lets you reflect on your study sessions, track distractions, and provide feedback.", "Surveys"});
    steps.append({tabRect("Analytics"), "The Analytics tab shows your study patterns, focus, and progress. Use the date range to filter your data and check the Study Insights for smart tips.", "Analytics"});
    steps.append({tabRect("Calendar"), "The Calendar tab helps you plan and review your study schedule. Click a day to use the Smart Planner and organize your goals.", "Calendar"});
    steps.append({tabRect("Settings"), "In the Settings tab, you can customize Pomodoro durations, notification preferences, and more.", "Settings"});
    steps.append({tabRect("Help"), "The Help tab provides answers to common questions and tips for getting the most out of StudyBuddy. You can always restart this walkthrough from here.", "Help"});
    walkthroughOverlay->setSteps(steps);
    walkthroughOverlay->onFinish = [this]() {
        if (walkthroughOverlay) walkthroughOverlay->hide();
    };
    walkthroughOverlay->show();
    walkthroughOverlay->raise();
    walkthroughOverlay->activateWindow();
    // Switch tab on overlay step change
    connect(walkthroughOverlay, &WalkthroughOverlay::stepChanged, this, [this, steps](int step) {
        if (step >= 0 && step < steps.size()) {
            QString tab = steps[step].tabName;
            for (int i = 0; i < tabWidget->count(); ++i) {
                if (tabWidget->tabText(i).toLower() == tab.toLower()) {
                    tabWidget->setCurrentIndex(i);
                    break;
                }
            }
        }
    });
    // Initial tab
    if (!steps.isEmpty()) {
        QString tab = steps[0].tabName;
        for (int i = 0; i < tabWidget->count(); ++i) {
            if (tabWidget->tabText(i).toLower() == tab.toLower()) {
                tabWidget->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MainWindow::loadApiKey() {
    QSettings settings("config.ini", QSettings::IniFormat);
    GEMINI_API_KEY = settings.value("API/GEMINI_API_KEY").toString();
    
    if (GEMINI_API_KEY.isEmpty()) {
        qDebug() << "Warning: GEMINI_API_KEY not found in config.ini";
        // Fallback to hardcoded key for development (remove in production)
        GEMINI_API_KEY = "AIzaSyBYkxeqX0en7k7aF4Huk1-5YBgIqdnBQBA";
    }
}
