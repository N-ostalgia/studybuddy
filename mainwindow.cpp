#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "analytics.h"
#include "achievements.h"
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


bool MainWindow::initializeDatabase()
{
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_default_connection");
    }

    db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QCoreApplication::applicationDirPath() + "/face_detection.db";
    db.setDatabaseName(dbPath);

    qDebug() << "Attempting to open database at:" << dbPath;

    if (!db.open()) {
        qDebug() << "Error: Unable to open database" << db.lastError().text();
        QMessageBox::critical(this, "Database Error", "Unable to open the database: " + db.lastError().text());
        return false;
    }

    qDebug() << "Database connected successfully at" << dbPath;

    QSqlQuery query(db);
    // Create the detections table
    QString createTableQuery =
        "CREATE TABLE IF NOT EXISTS detections ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp TEXT NOT NULL, "
        "face_count INTEGER NOT NULL, "
        "eyes_detected BOOLEAN NOT NULL, "
        "blink_count INTEGER NOT NULL, "
        "focus_score REAL NOT NULL)";

    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Failed to create table" << query.lastError().text();
        qDebug() << "Query was:" << createTableQuery;
        QMessageBox::critical(this, "Database Error", "Failed to create table: " + query.lastError().text());
        return false;
    }

    if (!db.tables().contains("detections")) {
        qDebug() << "Error: Table 'detections' was not created successfully";
        QMessageBox::critical(this, "Database Error", "Failed to verify table creation");
        return false;
    }

    qDebug() << "Database table 'detections' initialized successfully";
    qDebug() << "Available tables:" << db.tables();

    // Create the session_plans table
    QString createSessionPlansTableQuery =
        "CREATE TABLE IF NOT EXISTS session_plans (" 
        "id INTEGER PRIMARY KEY AUTOINCREMENT, " 
        "subject TEXT NOT NULL, " 
        "goals TEXT, " 
        "resource_link TEXT, " 
        "mental_state TEXT, " 
        "plan_date TEXT NOT NULL)";

    if (!query.exec(createSessionPlansTableQuery)) {
        qDebug() << "Error: Failed to create session_plans table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create session_plans table: " + query.lastError().text());
        return false;
    }

    qDebug() << "Database table 'session_plans' initialized successfully";

    // Create the session_reviews table
    QString createSessionReviewsTableQuery =
        "CREATE TABLE IF NOT EXISTS session_reviews (" 
        "id INTEGER PRIMARY KEY AUTOINCREMENT, " 
        "session_plan_id INTEGER NOT NULL, " 
        "focus_score REAL, " 
        "distraction_events TEXT, " 
        "effectiveness TEXT, " 
        "notes TEXT, " 
        "review_date TEXT NOT NULL, " 
        "FOREIGN KEY(session_plan_id) REFERENCES session_plans(id))";
QSqlQuery checkReviewColQuery(db);
checkReviewColQuery.exec("PRAGMA table_info(session_reviews)");
bool hasSessionId = false;
while (checkReviewColQuery.next()) {
    if (checkReviewColQuery.value(1).toString() == "session_id") {
        hasSessionId = true;
        break;
    }
}
if (!hasSessionId) {
    QSqlQuery alterQuery(db);
    alterQuery.exec("ALTER TABLE session_reviews ADD COLUMN session_id INTEGER");
}

    if (!query.exec(createSessionReviewsTableQuery)) {
        qDebug() << "Error: Failed to create session_reviews table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create session_reviews table: " + query.lastError().text());
        return false;
    }

    qDebug() << "Database table 'session_reviews' initialized successfully";

    // Add study_sessions table
    QString createStudySessionsTableQuery =
        "CREATE TABLE IF NOT EXISTS study_sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "planned_session_id INTEGER, "
        "start_time TEXT, "
        "end_time TEXT, "
        "type TEXT, "
        "notes TEXT)";
    if (!query.exec(createStudySessionsTableQuery)) {
        qDebug() << "Error: Failed to create study_sessions table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create study_sessions table: " + query.lastError().text());
        return false;
    }
    qDebug() << "Database table 'study_sessions' initialized successfully";

    // Create goal_resources table
    QString createGoalResourcesTableQuery =
        "CREATE TABLE IF NOT EXISTS goal_resources ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "goal_id INTEGER NOT NULL, "
        "type TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "description TEXT, "
        "FOREIGN KEY(goal_id) REFERENCES goals(id))";
    if (!query.exec(createGoalResourcesTableQuery)) {
        qDebug() << "Error: Failed to create goal_resources table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create goal_resources table: " + query.lastError().text());
        return false;
    }
    qDebug() << "Database table 'goal_resources' initialized successfully";

    // Create session_resources table
    QString createSessionResourcesTableQuery =
        "CREATE TABLE IF NOT EXISTS session_resources ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "session_id INTEGER NOT NULL, "
        "type TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "description TEXT, "
        "FOREIGN KEY(session_id) REFERENCES study_sessions(id))";
    if (!query.exec(createSessionResourcesTableQuery)) {
        qDebug() << "Error: Failed to create session_resources table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create session_resources table: " + query.lastError().text());
        return false;
    }
    qDebug() << "Database table 'session_resources' initialized successfully";

    // Create session_goals table
    QString createSessionGoalsTableQuery =
        "CREATE TABLE IF NOT EXISTS session_goals ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "session_id INTEGER NOT NULL, "
        "goal_id INTEGER NOT NULL, "
        "FOREIGN KEY(session_id) REFERENCES study_sessions(id), "
        "FOREIGN KEY(goal_id) REFERENCES goals(id))";
    if (!query.exec(createSessionGoalsTableQuery)) {
        qDebug() << "Error: Failed to create session_goals table" << query.lastError().text();
        QMessageBox::critical(this, "Database Error", "Failed to create session_goals table: " + query.lastError().text());
        return false;
    }
    qDebug() << "Database table 'session_goals' initialized successfully";

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
    studyGoal = new StudyGoal(this);
    studyStreak = new StudyStreak(this);
    pomodoroTimer = new PomodoroTimer(this);
    achievementTracker = new Achievement(this);
    alertSound = new QSoundEffect(this);

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
    createAnalyticsTab();
    createSettingsTab();
    createAchievementsTab();

    // Create toolbar
    QToolBar *toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("Start", this, &MainWindow::startWebcam);
    toolbar->addAction("Stop", this, &MainWindow::stopWebcam);
    toolbar->addAction("History", this, &MainWindow::showDetectionHistory);
    toolbar->addAction("Export", this, &MainWindow::exportData);
    toolbar->addAction("Settings", this, &MainWindow::showSettings);
    toolbar->addAction("Plan Session", this, &MainWindow::createSessionPlanner);
    toolbar->addAction("Review Session", this, &MainWindow::implementSessionReview);
    toolbar->addAction("Export All", this, &MainWindow::exportToCSV);
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
    // Goal Creation Section
    QGroupBox *createGoalGroup = new QGroupBox("Create New Goal");
    QFormLayout *createGoalLayout = new QFormLayout(createGoalGroup);

    goalSubjectInput = new QLineEdit();
    goalTargetMinutesInput = new QSpinBox();
    goalTargetMinutesInput->setRange(1, 1000);
    goalNotesInput = new QTextEdit();
    
    // category combo box
    goalCategoryCombo = new QComboBox();
    goalCategoryCombo->addItems({"Uncategorized", "Academic", "Personal", "Work", "Health", "Other"});

    // recurrence controls
    recurrenceTypeCombo = new QComboBox();
    recurrenceTypeCombo->addItems({"None", "Daily", "Weekly", "Monthly"});
    recurrenceValueInput = new QLineEdit();
    recurrenceValueInput->setPlaceholderText("e.g., 'Mon,Wed,Fri' for weekly");
    recurrenceValueInput->setEnabled(false);
    
    connect(recurrenceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                recurrenceValueInput->setEnabled(index > 0);
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

    // Current Goals List Section
    QGroupBox *currentGoalsGroup = new QGroupBox("Current Goals");
    QVBoxLayout *currentGoalsLayout = new QVBoxLayout(currentGoalsGroup);

    goalsListWidget = new QListWidget();
    deleteGoalButton = new QPushButton("Delete Goal");
    startTrackingButton = new QPushButton("Start Tracking Goal");
    stopTrackingButton = new QPushButton("Stop Tracking Goal");
    stopTrackingButton->setEnabled(false);

    QHBoxLayout *goalButtonsLayout = new QHBoxLayout();
    goalButtonsLayout->addWidget(startTrackingButton);
    goalButtonsLayout->addWidget(stopTrackingButton);
    goalButtonsLayout->addWidget(deleteGoalButton);

    currentGoalsLayout->addWidget(goalsListWidget);
    currentGoalsLayout->addLayout(goalButtonsLayout);

    parentLayout->addWidget(currentGoalsGroup);

    // Resource Management Section
    QGroupBox *resourceGroup = new QGroupBox("Resources");
    QVBoxLayout *resourceLayout = new QVBoxLayout(resourceGroup);

    // Add File
    QPushButton *addFileButton = new QPushButton("Add File");
    resourceLayout->addWidget(addFileButton);

    // Add Link
    QHBoxLayout *linkLayout = new QHBoxLayout();
    QLineEdit *linkInput = new QLineEdit();
    linkInput->setPlaceholderText("Paste link here...");
    QPushButton *addLinkButton = new QPushButton("Add Link");
    linkLayout->addWidget(linkInput);
    linkLayout->addWidget(addLinkButton);
    resourceLayout->addLayout(linkLayout);

    // Add Note
    QHBoxLayout *noteLayout = new QHBoxLayout();
    QTextEdit *noteInput = new QTextEdit();
    noteInput->setPlaceholderText("Add a note...");
    noteInput->setMaximumHeight(50);
    QPushButton *addNoteButton = new QPushButton("Add Note");
    noteLayout->addWidget(noteInput);
    noteLayout->addWidget(addNoteButton);
    resourceLayout->addLayout(noteLayout);

    // Resource List
    QListWidget *resourceList = new QListWidget();
    resourceLayout->addWidget(resourceList);

    createGoalLayout->addRow(resourceGroup);

    // Resource Management Logic
    auto refreshResourceList = [this, resourceList]() {
        resourceList->clear();
        int goalId = m_currentSelectedGoalId;
        if (goalId == -1) return;
        QSqlQuery query(db);
        query.prepare("SELECT id, type, value, description FROM goal_resources WHERE goal_id = ?");
        query.addBindValue(goalId);
        if (query.exec()) {
            while (query.next()) {
                QString type = query.value(1).toString();
                QString value = query.value(2).toString();
                QString desc = query.value(3).toString();
                QString display = type.toUpper() + ": " + (desc.isEmpty() ? value : desc);
                QListWidgetItem *item = new QListWidgetItem(display);
                item->setData(Qt::UserRole, query.value(0).toInt());
                item->setData(Qt::UserRole + 1, type);
                item->setData(Qt::UserRole + 2, value);
                resourceList->addItem(item);
            }
        }
    };

    connect(addFileButton, &QPushButton::clicked, this, [this, resourceList, refreshResourceList]() {
        int goalId = m_currentSelectedGoalId;
        if (goalId == -1) { QMessageBox::warning(this, "No Goal Selected", "Please select or create a goal first."); return; }
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Attach");
        if (!filePath.isEmpty()) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO goal_resources (goal_id, type, value) VALUES (?, 'file', ?)");
            query.addBindValue(goalId);
            query.addBindValue(filePath);
            query.exec();
            refreshResourceList();
        }
    });
    connect(addLinkButton, &QPushButton::clicked, this, [this, linkInput, resourceList, refreshResourceList]() {
        int goalId = m_currentSelectedGoalId;
        QString link = linkInput->text().trimmed();
        if (goalId == -1 || link.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO goal_resources (goal_id, type, value) VALUES (?, 'link', ?)");
        query.addBindValue(goalId);
        query.addBindValue(link);
        query.exec();
        linkInput->clear();
        refreshResourceList();
    });
    connect(addNoteButton, &QPushButton::clicked, this, [this, noteInput, resourceList, refreshResourceList]() {
        int goalId = m_currentSelectedGoalId;
        QString note = noteInput->toPlainText().trimmed();
        if (goalId == -1 || note.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO goal_resources (goal_id, type, value) VALUES (?, 'note', ?)");
        query.addBindValue(goalId);
        query.addBindValue(note);
        query.exec();
        noteInput->clear();
        refreshResourceList();
    });
    connect(resourceList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString type = item->data(Qt::UserRole + 1).toString();
        QString value = item->data(Qt::UserRole + 2).toString();
        if (type == "file" || type == "link") {
            QDesktopServices::openUrl(QUrl::fromLocalFile(value));
        } else if (type == "note") {
            QMessageBox::information(this, "Note", value);
        }
    });

    // Refresh resource list when a goal is selected
    connect(goalsListWidget, &QListWidget::itemSelectionChanged, this, [refreshResourceList]() {
        refreshResourceList();
    });

    // Connect buttons to slots
    connect(addGoalButton, &QPushButton::clicked, this, [this, addFileButton, addLinkButton, addNoteButton]() {
        QString subject = goalSubjectInput->text();
        int targetMinutes = goalTargetMinutesInput->value();
        QString notes = goalNotesInput->toPlainText();
        QString recurrenceType = recurrenceTypeCombo->currentText();
        QString recurrenceValue = recurrenceValueInput->text();
        QString category = goalCategoryCombo->currentText();

        if (subject.isEmpty() || targetMinutes <= 0) {
            QMessageBox::warning(this, "Input Error", "Subject and Target Minutes cannot be empty.");
            return;
        }

        if (m_currentSelectedGoalId != -1) {
            // Update existing goal
            if (studyGoal->updateGoal(m_currentSelectedGoalId, subject, targetMinutes, notes, recurrenceType, recurrenceValue, category)) {
                QMessageBox::information(this, "Success", "Goal updated successfully!");
                handleCancelEdit();
            } else {
                QMessageBox::critical(this, "Error", "Failed to update goal.");
            }
        } else {
            // Create new goal
            if (studyGoal->createGoal(subject, targetMinutes, notes, recurrenceType, recurrenceValue, category)) {
                QMessageBox::information(this, "Success", "Goal created successfully!");
                refreshGoalsDisplay();

                if (this->goalsListWidget->count() > 0) {
                    QListWidgetItem *lastItem = this->goalsListWidget->item(this->goalsListWidget->count() - 1);
                    this->goalsListWidget->setCurrentItem(lastItem);
                }

            } else {
                QMessageBox::critical(this, "Error", "Failed to create goal.");
            }
        }
    });
    
    connect(deleteGoalButton, &QPushButton::clicked, this, &MainWindow::handleDeleteGoal);
    connect(startTrackingButton, &QPushButton::clicked, this, &MainWindow::handleStartGoalTracking);
    connect(stopTrackingButton, &QPushButton::clicked, this, &MainWindow::handleStopGoalTracking);
    connect(cancelEditButton, &QPushButton::clicked, this, &MainWindow::handleCancelEdit);

    // Connect list selection to fill input fields and enable/disable tracking buttons
    connect(goalsListWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        if (goalsListWidget->currentItem()) {
            m_currentSelectedGoalId = goalsListWidget->currentItem()->data(Qt::UserRole).toInt();
            QMap<QString, QVariant> details = studyGoal->getGoalDetails(m_currentSelectedGoalId);
            goalSubjectInput->setText(details["subject"].toString());
            goalTargetMinutesInput->setValue(details["target_minutes"].toInt());
            goalNotesInput->setText(details["notes"].toString());
            
            int recurrenceIndex = recurrenceTypeCombo->findText(details["recurrence_type"].toString());
            if (recurrenceIndex != -1) {
                recurrenceTypeCombo->setCurrentIndex(recurrenceIndex);
            }
            recurrenceValueInput->setText(details["recurrence_value"].toString());

            int categoryIndex = goalCategoryCombo->findText(details["category"].toString());
            if (categoryIndex != -1) {
                goalCategoryCombo->setCurrentIndex(categoryIndex);
            }

            addGoalButton->setText("Update Goal");
            cancelEditButton->setEnabled(true);
            startTrackingButton->setEnabled(true);
        } else {
            handleCancelEdit();
        }
    });


    refreshGoalsDisplay();

    resourceList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(resourceList, &QListWidget::customContextMenuRequested, this, [this, resourceList, refreshResourceList](const QPoint &pos) {
        QListWidgetItem *item = resourceList->itemAt(pos);
        if (!item) return;
        QMenu menu;
        QAction *deleteAction = menu.addAction("Delete Resource");
        QAction *selectedAction = menu.exec(resourceList->viewport()->mapToGlobal(pos));
        if (selectedAction == deleteAction) {
            int resourceId = item->data(Qt::UserRole).toInt();
            QSqlQuery query(db);
            query.prepare("DELETE FROM goal_resources WHERE id = ?");
            query.addBindValue(resourceId);
            query.exec();
            refreshResourceList();
        }
    });

    connect(goalsListWidget, &QListWidget::itemSelectionChanged, this, [this, addFileButton, addLinkButton, addNoteButton, refreshResourceList]() {
        bool hasSelection = goalsListWidget->currentItem() != nullptr && goalsListWidget->currentItem()->data(Qt::UserRole).isValid();
        addFileButton->setEnabled(hasSelection);
        addLinkButton->setEnabled(hasSelection);
        addNoteButton->setEnabled(hasSelection);
        refreshResourceList();
    });
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
    QChartView *productivityChartView = new QChartView(analytics->createProductivityChart(startDateEdit->date(), endDateEdit->date()));
    productivityChartView->setMinimumSize(500, 250);
    productivityChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QChartView *effectivenessChartView = new QChartView(analytics->createEffectivenessChart(startDateEdit->date(), endDateEdit->date()));
    effectivenessChartView->setMinimumSize(500, 250);
    effectivenessChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QChartView *distractionChartView = new QChartView(analytics->createDistractionChart(startDateEdit->date(), endDateEdit->date()));
    distractionChartView->setMinimumSize(500, 250);
    distractionChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QChartView *subjectChartView = new QChartView(analytics->createSubjectDistributionChart(startDateEdit->date(), endDateEdit->date()));
    subjectChartView->setMinimumSize(500, 250);
    subjectChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QChartView *goalChartView = new QChartView(analytics->createGoalCompletionChart(startDateEdit->date(), endDateEdit->date()));
    goalChartView->setMinimumSize(500, 250);
    goalChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QChartView *streakChartView = new QChartView(analytics->createStreakChart(startDateEdit->date(), endDateEdit->date()));
    streakChartView->setMinimumSize(500, 250);
    streakChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chartsLayout->addWidget(focusChartView);
    chartsLayout->addWidget(productivityChartView);
    chartsLayout->addWidget(effectivenessChartView);
    chartsLayout->addWidget(distractionChartView);
    chartsLayout->addWidget(subjectChartView);
    chartsLayout->addWidget(goalChartView);
    chartsLayout->addWidget(streakChartView);
    analyticsLayout->addLayout(chartsLayout);

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
        effectivenessChartView->setChart(analytics->createEffectivenessChart(startDateEdit->date(), endDateEdit->date()));
        distractionChartView->setChart(analytics->createDistractionChart(startDateEdit->date(), endDateEdit->date()));
        subjectChartView->setChart(analytics->createSubjectDistributionChart(startDateEdit->date(), endDateEdit->date()));
        goalChartView->setChart(analytics->createGoalCompletionChart(startDateEdit->date(), endDateEdit->date()));
        streakChartView->setChart(analytics->createStreakChart(startDateEdit->date(), endDateEdit->date()));
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
    
    alertLayout->addWidget(enableAlertsCheckbox);
    layout->addWidget(alertGroup);

    // Connect signals
    connect(enableAlertsCheckbox, &QCheckBox::toggled, [this](bool checked) {
        alertsEnabled = checked;
        saveSettings();
    });

    tabWidget->addTab(settingsTab, "Settings");
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
        QListWidgetItem *item = new QListWidgetItem(goal.subject + " (" + goal.category + ")");
        item->setData(Qt::UserRole, goal.id);
        goalList->addItem(item);
    }
    goalLayout->addWidget(goalList);
    QDialogButtonBox *goalBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    goalLayout->addWidget(goalBox);
    connect(goalBox, &QDialogButtonBox::accepted, &goalDialog, &QDialog::accept);
    connect(goalBox, &QDialogButtonBox::rejected, &goalDialog, &QDialog::reject);
    if (goalDialog.exec() != QDialog::Accepted) {
        return;
    }
    for (QListWidgetItem *item : goalList->selectedItems()) {
        selectedGoalIds.append(item->data(Qt::UserRole).toInt());
    }
    // End goal selection dialog

    if (!capture.open(0)) {
        QMessageBox::critical(this, "Error", "Could not open the webcam!");
        return;
    }

    // Create a new study session record for this webcam session
    int sessionId = studySession->createSession(-1, "webcam", "Webcam session started");
    studySession->setCurrentSessionId(sessionId);

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
    timer->start(33);
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
}

void MainWindow::stopWebcam()
{
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
                studyGoal->addProgress(goalId, durationMinutes);
            }
        }

    }
}

void MainWindow::updateFrame()
{
    capture.read(frame);

    if (frame.empty()) {
        QMessageBox::warning(this, "Warning", "Empty frame received from webcam!");
        stopWebcam();
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
    }

    displayImage(frame);
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
    if (!QSqlDatabase::database().isOpen()) {
        qDebug() << "Error: Database is not open when trying to log detection";
        return;
    }

    QSqlQuery query;
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
    QSqlQuery query;

    if (!QSqlDatabase::database().isOpen()) {
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
    QSqlQuery statsQuery;
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

    QMessageBox msgBox;
    msgBox.setWindowTitle("Detection History");
    msgBox.setText(history);
    msgBox.setStyleSheet("QMessageBox { min-width: 450px; } QLabel { font-family: 'Courier New', monospace; }");
    msgBox.exec();
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
        qDebug() << "Chart series not initialized";
        return;
    }

    try {
        focusSeries->clear();
        blinkSeries->clear();
        sessionTimer.restart();
        
        // Reset axes
        if (timeAxis) timeAxis->setRange(0, 1);
        if (focusAxis) focusAxis->setRange(0, 100);
        if (blinkAxis) blinkAxis->setRange(0, 30);

        // Force update
        if (focusChartView) focusChartView->update();
        if (blinkChartView) blinkChartView->update();
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

    // You can add a sound effect or a message box here
    QMessageBox::information(this, "Pomodoro", "Timer completed!");
}

void MainWindow::handleAddGoal()
{
}

void MainWindow::handleDeleteGoal()
{
    QListWidgetItem *selectedItem = goalsListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "Selection Error", "Please select a goal to delete.");
        return;
    }

    int goalId = selectedItem->data(Qt::UserRole).toInt();

    if (QMessageBox::question(this, "Confirm Deletion", "Are you sure you want to delete this goal?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (studyGoal->deleteGoal(goalId)) {
            QMessageBox::information(this, "Success", "Goal deleted successfully!");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete goal.");
        }
    }
}

void MainWindow::handleStartGoalTracking()
{
    QListWidgetItem *selectedItem = goalsListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "Selection Error", "Please select a goal to start tracking.");
        return;
    }
    int goalId = selectedItem->data(Qt::UserRole).toInt();
    int initialElapsedSeconds = m_currentSessionProgress.value(goalId, 0);
    studyGoal->startTrackingGoal(goalId, initialElapsedSeconds);
    pomodoroTimer->start();
}

void MainWindow::handleStopGoalTracking()
{
    pomodoroTimer->pause();
    startTrackingButton->setEnabled(true);
    stopTrackingButton->setEnabled(false);
}

void MainWindow::refreshGoalsDisplay()
{
    goalsListWidget->clear();
    QList<GoalInfo> goals = studyGoal->getGoalsForDate(QDate::currentDate());

    if (goals.isEmpty()) {
        goalsListWidget->addItem("No goals for today.");
        return;
    }

    for (const auto &goal : goals) {
        int completedMinutes = goal.completedMinutes;
        int targetMinutes = goal.targetMinutes;
        float completionPercentage = studyGoal->getCompletionPercentage(goal.id);
        QString recurrenceType = goal.recurrenceType;
        QString recurrenceValue = goal.recurrenceValue;


        if (goal.id == m_currentActiveGoalId && m_currentSessionProgress.contains(goal.id)) {
            completedMinutes += (m_currentSessionProgress.value(goal.id) / 60);
            if (targetMinutes > 0) {
                completionPercentage = (float)completedMinutes / targetMinutes * 100.0f;
            }
        }

        // Build the display string with recurrence information
        QString display = QString("%1 (%2): %3/%4 minutes (%5%)")
                              .arg(goal.subject)
                              .arg(goal.category)
                              .arg(completedMinutes)
                              .arg(targetMinutes)
                              .arg(completionPercentage, 0, 'f', 1);
        
        // Add recurrence information if it's a recurring goal
        if (recurrenceType != "None") {
            display += QString(" [%1").arg(recurrenceType);
            if (!recurrenceValue.isEmpty()) {
                display += QString(": %1").arg(recurrenceValue);
            }
            display += "]";
        }

        QListWidgetItem *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, goal.id);
        goalsListWidget->addItem(item);
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

    for (int i = 0; i < goalsListWidget->count(); ++i) {
        QListWidgetItem *item = goalsListWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == goalId) {
            GoalInfo details;
            QList<GoalInfo> goalsForDate = studyGoal->getGoalsForDate(QDate::currentDate());
            for (const auto& g : goalsForDate) {
                if (g.id == goalId) {
                    details = g;
                    break;
                }
            }
            if (details.id == -1) { 
                QMap<QString, QVariant> qmapDetails = studyGoal->getGoalDetails(goalId);
                details.subject = qmapDetails["subject"].toString();
                details.completedMinutes = qmapDetails["completed_minutes"].toInt();
                details.targetMinutes = qmapDetails["target_minutes"].toInt();
                details.recurrenceType = qmapDetails["recurrence_type"].toString();
                details.recurrenceValue = qmapDetails["recurrence_value"].toString();
                details.category = qmapDetails["category"].toString();
            }


            int completedMinutesFromDB = details.completedMinutes;
            int totalTargetMinutes = details.targetMinutes;

            // Calculate current session minutes and seconds
            int currentSessionMinutes = elapsedSeconds / 60;
            int currentSessionSeconds = elapsedSeconds % 60;

            // Calculate total minutes for display (from DB + current session)
            int totalCompletedMinutesForDisplay = completedMinutesFromDB + currentSessionMinutes;

            float completionPercentage = 0.0f;
            if (totalTargetMinutes > 0) {
                completionPercentage = (float)totalCompletedMinutesForDisplay / totalTargetMinutes * 100.0f;
            }

            // Update the item text to show real-time progress, including seconds for current session
            QString recurrenceType = details.recurrenceType;
            QString recurrenceValue = details.recurrenceValue;
            QString category = details.category;

            QString display = QString("%1 (%2): %3/%4 minutes (%5%) (Current Session: %6:%7)")
                                  .arg(details.subject)
                                  .arg(category)
                                  .arg(totalCompletedMinutesForDisplay)
                                  .arg(totalTargetMinutes)
                                  .arg(completionPercentage, 0, 'f', 1)
                                  .arg(currentSessionMinutes, 2, 10, QChar('0'))
                                  .arg(currentSessionSeconds, 2, 10, QChar('0'));

            if (recurrenceType != "None") {
                display += QString(" [%1").arg(recurrenceType);
                if (!recurrenceValue.isEmpty()) {
                    display += QString(": %1").arg(recurrenceValue);
                }
                display += "]";
            }
            item->setText(display);
            break;
        }
    }
}

void MainWindow::onGoalSessionProgressUpdated(int goalId, int sessionMinutes)
{
    Q_UNUSED(sessionMinutes);
    qDebug() << "MainWindow: Goal" << goalId << "completed session. Refreshing display.";
    m_currentSessionProgress.remove(goalId);
    refreshGoalsDisplay();

    // Check if goal is completed
    float completionPercentage = studyGoal->getCompletionPercentage(goalId);
    if (completionPercentage >= 100.0f) {
        QMessageBox::information(this, "Goal Completed!",
                                 QString("Congratulations! You have completed your goal for '%1'!").arg(studyGoal->getGoalDetails(goalId)["subject"].toString()));
    }
}

void MainWindow::onGoalTrackingStarted(int goalId)
{
    Q_UNUSED(goalId);
    m_currentActiveGoalId = goalId;
    startTrackingButton->setEnabled(false);
    stopTrackingButton->setEnabled(true);
    QMessageBox::information(this, "Goal Tracking", "Tracking started for selected goal.");
}

void MainWindow::onGoalTrackingStopped()
{
    startTrackingButton->setEnabled(true);
    stopTrackingButton->setEnabled(false);
}

void MainWindow::handleGoalDeleted(int goalId)
{
    Q_UNUSED(goalId);
    if (m_currentActiveGoalId == goalId) {
        handleStopGoalTracking();
    }
    refreshGoalsDisplay();
}

void MainWindow::onAchievementUnlocked(const QString &achievementId, const QString &name)
{
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
    startTrackingButton->setEnabled(false);
    stopTrackingButton->setEnabled(false);
    goalsListWidget->clearSelection();
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

void MainWindow::createSessionPlanner()
{
    QDialog *sessionPlannerDialog = new QDialog(this);
    sessionPlannerDialog->setWindowTitle("Plan Study Session");
    sessionPlannerDialog->setMinimumSize(400, 300);

    QVBoxLayout *mainLayout = new QVBoxLayout(sessionPlannerDialog);
    QFormLayout *formLayout = new QFormLayout();

    // Goal Selection Section
    QListWidget *goalSelectList = new QListWidget();
    QGroupBox *goalSelectGroup = new QGroupBox("Select Goals for this Session");
    QVBoxLayout *goalSelectLayout = new QVBoxLayout(goalSelectGroup);
    goalSelectList->setSelectionMode(QAbstractItemView::MultiSelection);
    QList<GoalInfo> allGoals = studyGoal->getGoalsForDate(QDate::currentDate());
    for (const auto &goal : allGoals) {
        QListWidgetItem *item = new QListWidgetItem(goal.subject + " (" + goal.category + ")");
        item->setData(Qt::UserRole, goal.id);
        goalSelectList->addItem(item);
    }
    goalSelectLayout->addWidget(goalSelectList);
    mainLayout->addWidget(goalSelectGroup);

    // Subject/Topic
    QLineEdit *subjectInput = new QLineEdit();
    subjectInput->setPlaceholderText("e.g., Quantum Physics, French Grammar");
    formLayout->addRow("Subject/Topic:", subjectInput);

    // Session Goals/Objectives
    QTextEdit *goalsInput = new QTextEdit();
    goalsInput->setPlaceholderText("e.g., Understand Chapter 5, Memorize 20 new words");
    formLayout->addRow("Session Goals:", goalsInput);

    // Resource Linking
    QHBoxLayout *resourceLayout = new QHBoxLayout();
    QLineEdit *resourceLinkInput = new QLineEdit();
    resourceLinkInput->setPlaceholderText("Select a file or enter a URL");
    QPushButton *browseButton = new QPushButton("Browse...");
    resourceLayout->addWidget(resourceLinkInput);
    resourceLayout->addWidget(browseButton);
    formLayout->addRow("Resource Link:", resourceLayout);

    connect(browseButton, &QPushButton::clicked, this, [resourceLinkInput]() {
        QString filePath = QFileDialog::getOpenFileName(nullptr, "Select Resource File");
        if (!filePath.isEmpty()) {
            resourceLinkInput->setText(filePath);
        }
    });

    // Pre-session Mental State Assessment
    QComboBox *mentalStateCombo = new QComboBox();
    mentalStateCombo->addItems({"Excellent", "Good", "Neutral", "Tired", "Distracted"});
    formLayout->addRow("Mental State:", mentalStateCombo);

    mainLayout->addLayout(formLayout);

    //  Resource Management Section for Session Planner
    QGroupBox *resourceGroupPlanner = new QGroupBox("Resources");
    QVBoxLayout *resourceLayoutPlanner = new QVBoxLayout(resourceGroupPlanner);
    QPushButton *addFileButtonPlanner = new QPushButton("Add File");
    resourceLayoutPlanner->addWidget(addFileButtonPlanner);
    QHBoxLayout *linkLayoutPlanner = new QHBoxLayout();
    QLineEdit *linkInputPlanner = new QLineEdit();
    linkInputPlanner->setPlaceholderText("Paste link here...");
    QPushButton *addLinkButtonPlanner = new QPushButton("Add Link");
    linkLayoutPlanner->addWidget(linkInputPlanner);
    linkLayoutPlanner->addWidget(addLinkButtonPlanner);
    resourceLayoutPlanner->addLayout(linkLayoutPlanner);
    QHBoxLayout *noteLayoutPlanner = new QHBoxLayout();
    QTextEdit *noteInputPlanner = new QTextEdit();
    noteInputPlanner->setPlaceholderText("Add a note...");
    noteInputPlanner->setMaximumHeight(50);
    QPushButton *addNoteButtonPlanner = new QPushButton("Add Note");
    noteLayoutPlanner->addWidget(noteInputPlanner);
    noteLayoutPlanner->addWidget(addNoteButtonPlanner);
    resourceLayoutPlanner->addLayout(noteLayoutPlanner);
    QListWidget *resourceListPlanner = new QListWidget();
    resourceLayoutPlanner->addWidget(resourceListPlanner);
    mainLayout->addWidget(resourceGroupPlanner);

    int tempSessionId = -1;
    auto refreshResourceList = [this, resourceListPlanner, &tempSessionId]() {
        resourceListPlanner->clear();
        if (tempSessionId == -1) return;
        QSqlQuery query(db);
        query.prepare("SELECT id, type, value, description FROM session_resources WHERE session_id = ?");
        query.addBindValue(tempSessionId);
        if (query.exec()) {
            while (query.next()) {
                QString type = query.value(1).toString();
                QString value = query.value(2).toString();
                QString desc = query.value(3).toString();
                QString display = type.toUpper() + ": " + (desc.isEmpty() ? value : desc);
                QListWidgetItem *item = new QListWidgetItem(display);
                item->setData(Qt::UserRole, query.value(0).toInt());
                item->setData(Qt::UserRole + 1, type);
                item->setData(Qt::UserRole + 2, value);
                resourceListPlanner->addItem(item);
            }
        }
    };

    connect(addFileButtonPlanner, &QPushButton::clicked, this, [this, &tempSessionId, refreshResourceList]() {
        if (tempSessionId == -1) { QMessageBox::warning(this, "No Session Saved", "Please save the session plan first."); return; }
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Attach");
        if (!filePath.isEmpty()) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'file', ?)");
            query.addBindValue(tempSessionId);
            query.addBindValue(filePath);
            query.exec();
            refreshResourceList();
        }
    });
    connect(addLinkButtonPlanner, &QPushButton::clicked, this, [this, linkInputPlanner, &tempSessionId, refreshResourceList]() {
        QString link = linkInputPlanner->text().trimmed();
        if (tempSessionId == -1 || link.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'link', ?)");
        query.addBindValue(tempSessionId);
        query.addBindValue(link);
        query.exec();
        linkInputPlanner->clear();
        refreshResourceList();
    });
    connect(addNoteButtonPlanner, &QPushButton::clicked, this, [this, noteInputPlanner, &tempSessionId, refreshResourceList]() {
        QString note = noteInputPlanner->toPlainText().trimmed();
        if (tempSessionId == -1 || note.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'note', ?)");
        query.addBindValue(tempSessionId);
        query.addBindValue(note);
        query.exec();
        noteInputPlanner->clear();
        refreshResourceList();
    });
    connect(resourceListPlanner, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString type = item->data(Qt::UserRole + 1).toString();
        QString value = item->data(Qt::UserRole + 2).toString();
        if (type == "file" || type == "link") {
            QDesktopServices::openUrl(QUrl::fromLocalFile(value));
        } else if (type == "note") {
            QMessageBox::information(this, "Note", value);
        }
    });

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal, sessionPlannerDialog);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, sessionPlannerDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, sessionPlannerDialog, &QDialog::reject);

    QList<int> selectedGoalIds;

    if (sessionPlannerDialog->exec() == QDialog::Accepted) {
        QString subject = subjectInput->text();
        QString goals = goalsInput->toPlainText();
        QString resourceLink = resourceLinkInput->text();
        QString mentalState = mentalStateCombo->currentText();
        QString planDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        QSqlQuery query;
        query.prepare("INSERT INTO session_plans (subject, goals, resource_link, mental_state, plan_date) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(subject);
        query.addBindValue(goals);
        query.addBindValue(resourceLink);
        query.addBindValue(mentalState);
        query.addBindValue(planDate);

        if (!query.exec()) {
            qDebug() << "Error saving session plan:" << query.lastError().text();
            QMessageBox::critical(this, "Database Error", "Failed to save session plan: " + query.lastError().text());
        } else {
            int plannedSessionId = query.lastInsertId().toInt();
            // Create a study_sessions record for this planned session
            int sessionId = studySession->createSession(plannedSessionId, "planned", "Planned session created");
            studySession->endSession(sessionId);
            // Save session-goal links
            selectedGoalIds.clear();
            for (QListWidgetItem *item : goalSelectList->selectedItems()) {
                selectedGoalIds.append(item->data(Qt::UserRole).toInt());
            }
            if (!query.exec()) {
                qDebug() << "Error saving session plan:" << query.lastError().text();
                QMessageBox::critical(this, "Database Error", "Failed to save session plan: " + query.lastError().text());
            } else {
                int plannedSessionId = query.lastInsertId().toInt();
                // Create a study_sessions record for this planned session
                int sessionId = studySession->createSession(plannedSessionId, "planned", "Planned session created");
                studySession->endSession(sessionId);
                //  Save session-goal links
                for (int goalId : selectedGoalIds) {
                    QSqlQuery linkQuery(db);
                    linkQuery.prepare("INSERT INTO session_goals (session_id, goal_id) VALUES (?, ?)");
                    linkQuery.addBindValue(sessionId);
                    linkQuery.addBindValue(goalId);
                    linkQuery.exec();
                }
                qDebug() << "Session Plan and StudySession Saved to DB:";
                qDebug() << "  Subject:" << subject;
                qDebug() << "  Goals:" << goals;
                qDebug() << "  Resource Link:" << resourceLink;
                qDebug() << "  Mental State:" << mentalState;
                QMessageBox::information(this, "Session Planned", "Your study session has been planned and saved!");
                tempSessionId = sessionId;
                refreshResourceList();
            }
        }
    } else {
        QMessageBox::information(this, "Session Planning", "Session planning cancelled.");
    }
    sessionPlannerDialog->deleteLater();


    resourceListPlanner->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(resourceListPlanner, &QListWidget::customContextMenuRequested, this, [this, resourceListPlanner, refreshResourceList](const QPoint &pos) {
        QListWidgetItem *item = resourceListPlanner->itemAt(pos);
        if (!item) return;
        QMenu menu;
        QAction *deleteAction = menu.addAction("Delete Resource");
        QAction *selectedAction = menu.exec(resourceListPlanner->viewport()->mapToGlobal(pos));
        if (selectedAction == deleteAction) {
            int resourceId = item->data(Qt::UserRole).toInt();
            QSqlQuery query(db);
            query.prepare("DELETE FROM session_resources WHERE id = ?");
            query.addBindValue(resourceId);
            query.exec();
            refreshResourceList();
        }
    });
}

void MainWindow::implementSessionReview()
{
    QDialog *sessionReviewDialog = new QDialog(this);
    sessionReviewDialog->setWindowTitle("Session Review");
    sessionReviewDialog->setMinimumSize(450, 400);

    // Create main container widget and layout
    QWidget *container = new QWidget();
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    
    // Create scroll area
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(container);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(sessionReviewDialog);
    mainLayout->addWidget(scrollArea);
    QFormLayout *formLayout = new QFormLayout();

    // Retrieve the most recent session plan
    QSqlQuery planQuery;
    planQuery.exec("SELECT id, subject, goals FROM session_plans ORDER BY plan_date DESC LIMIT 1");
    int currentSessionPlanId = -1;
    QString plannedSubject = "N/A";
    QString plannedGoals = "N/A";

    if (planQuery.next()) {
        currentSessionPlanId = planQuery.value("id").toInt();
        plannedSubject = planQuery.value("subject").toString();
        plannedGoals = planQuery.value("goals").toString();
    }

    QLabel *planSummaryLabel = new QLabel(QString("<b>Last Planned Session:</b><br/>Subject: %1<br/>Goals: %2").arg(plannedSubject, plannedGoals));
    planSummaryLabel->setWordWrap(true);
    formLayout->addRow("", planSummaryLabel);

    // Display Current Session Metrics
    QLabel *sessionDurationReviewLabel = new QLabel(QString("Duration: %1").arg(sessionDurationLabel->text().remove("Duration: ")));
    QLabel *blinkCountReviewLabel = new QLabel(QString("Total Blinks: %1").arg(blinkCount));
    QLabel *focusScoreReviewLabel = new QLabel(QString("Average Focus: %1%").arg(QString::number(currentFocusScore, 'f', 1)));

    formLayout->addRow("Current Session Stats:", sessionDurationReviewLabel);
    formLayout->addRow("", blinkCountReviewLabel);
    formLayout->addRow("", focusScoreReviewLabel);

    // Self-reported Effectiveness
    QComboBox *effectivenessCombo = new QComboBox();
    effectivenessCombo->addItems({"Highly Effective", "Moderately Effective", "Neutral", "Slightly Ineffective", "Highly Ineffective"});
    formLayout->addRow("Effectiveness:", effectivenessCombo);

    // Distraction Events
    QTextEdit *distractionEventsInput = new QTextEdit();
    distractionEventsInput->setPlaceholderText("Describe any distractions encountered...");
    distractionEventsInput->setMaximumHeight(100);
    formLayout->addRow("Distraction Events:", distractionEventsInput);

    // Notes/Learnings
    QTextEdit *notesInput = new QTextEdit();
    notesInput->setPlaceholderText("Enter your notes, learnings, or observations from the session...");
    notesInput->setMaximumHeight(100);
    formLayout->addRow("Notes/Learnings:", notesInput);

    containerLayout->addLayout(formLayout);

    // Resource Management Section for Session Review
    QGroupBox *resourceGroupReview = new QGroupBox("Resources");
    QVBoxLayout *resourceLayoutReview = new QVBoxLayout(resourceGroupReview);
    QPushButton *addFileButtonReview = new QPushButton("Add File");
    resourceLayoutReview->addWidget(addFileButtonReview);
    QHBoxLayout *linkLayoutReview = new QHBoxLayout();
    QLineEdit *linkInputReview = new QLineEdit();
    linkInputReview->setPlaceholderText("Paste link here...");
    QPushButton *addLinkButtonReview = new QPushButton("Add Link");
    linkLayoutReview->addWidget(linkInputReview);
    linkLayoutReview->addWidget(addLinkButtonReview);
    resourceLayoutReview->addLayout(linkLayoutReview);
    QHBoxLayout *noteLayoutReview = new QHBoxLayout();
    QTextEdit *noteInputReview = new QTextEdit();
    noteInputReview->setPlaceholderText("Add a note...");
    noteInputReview->setMaximumHeight(50);
    QPushButton *addNoteButtonReview = new QPushButton("Add Note");
    noteLayoutReview->addWidget(noteInputReview);
    noteLayoutReview->addWidget(addNoteButtonReview);
    resourceLayoutReview->addLayout(noteLayoutReview);
    QListWidget *resourceListReview = new QListWidget();
    resourceListReview->setMaximumHeight(150);
    resourceLayoutReview->addWidget(resourceListReview);
    containerLayout->addWidget(resourceGroupReview);

    int tempSessionId = -1;
    auto refreshResourceList = [this, resourceListReview, &tempSessionId]() {
        resourceListReview->clear();
        if (tempSessionId == -1) return;
        QSqlQuery query(db);
        query.prepare("SELECT id, type, value, description FROM session_resources WHERE session_id = ?");
        query.addBindValue(tempSessionId);
        if (query.exec()) {
            while (query.next()) {
                QString type = query.value(1).toString();
                QString value = query.value(2).toString();
                QString desc = query.value(3).toString();
                QString display = type.toUpper() + ": " + (desc.isEmpty() ? value : desc);
                QListWidgetItem *item = new QListWidgetItem(display);
                item->setData(Qt::UserRole, query.value(0).toInt());
                item->setData(Qt::UserRole + 1, type);
                item->setData(Qt::UserRole + 2, value);
                resourceListReview->addItem(item);
            }
        }
    };

    connect(addFileButtonReview, &QPushButton::clicked, this, [this, &tempSessionId, refreshResourceList]() {
        if (tempSessionId == -1) { QMessageBox::warning(this, "No Session Saved", "Please save the session plan first."); return; }
        QString filePath = QFileDialog::getOpenFileName(this, "Select File to Attach");
        if (!filePath.isEmpty()) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'file', ?)");
            query.addBindValue(tempSessionId);
            query.addBindValue(filePath);
            query.exec();
            refreshResourceList();
        }
    });
    connect(addLinkButtonReview, &QPushButton::clicked, this, [this, linkInputReview, &tempSessionId, refreshResourceList]() {
        QString link = linkInputReview->text().trimmed();
        if (tempSessionId == -1 || link.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'link', ?)");
        query.addBindValue(tempSessionId);
        query.addBindValue(link);
        query.exec();
        linkInputReview->clear();
        refreshResourceList();
    });
    connect(addNoteButtonReview, &QPushButton::clicked, this, [this, noteInputReview, &tempSessionId, refreshResourceList]() {
        QString note = noteInputReview->toPlainText().trimmed();
        if (tempSessionId == -1 || note.isEmpty()) return;
        QSqlQuery query(db);
        query.prepare("INSERT INTO session_resources (session_id, type, value) VALUES (?, 'note', ?)");
        query.addBindValue(tempSessionId);
        query.addBindValue(note);
        query.exec();
        noteInputReview->clear();
        refreshResourceList();
    });
    connect(resourceListReview, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString type = item->data(Qt::UserRole + 1).toString();
        QString value = item->data(Qt::UserRole + 2).toString();
        if (type == "file" || type == "link") {
            QDesktopServices::openUrl(QUrl::fromLocalFile(value));
        } else if (type == "note") {
            QMessageBox::information(this, "Note", value);
        }
    });

    containerLayout->addStretch();

    // Buttons - outside the scroll area
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal, sessionReviewDialog);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, sessionReviewDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, sessionReviewDialog, &QDialog::reject);

    if (sessionReviewDialog->exec() == QDialog::Accepted) {
        QString effectiveness = effectivenessCombo->currentText();
        QString distractions = distractionEventsInput->toPlainText();
        QString notes = notesInput->toPlainText();
        QString reviewDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        // Attach review to the current or most recent study session
        int sessionId = studySession->getCurrentSessionId();
        if (sessionId == -1) {
            // Get the most recent session
            QSqlQuery lastSessionQuery;
            lastSessionQuery.exec("SELECT id FROM study_sessions ORDER BY end_time DESC LIMIT 1");
            if (lastSessionQuery.next()) {
                sessionId = lastSessionQuery.value(0).toInt();
            }
        }

        QSqlQuery reviewQuery;
        reviewQuery.prepare("INSERT INTO session_reviews (session_plan_id, focus_score, distraction_events, effectiveness, notes, review_date, session_id) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        reviewQuery.addBindValue(currentSessionPlanId);
        reviewQuery.addBindValue(currentFocusScore);
        reviewQuery.addBindValue(distractions);
        reviewQuery.addBindValue(effectiveness);
        reviewQuery.addBindValue(notes);
        reviewQuery.addBindValue(reviewDate);
        reviewQuery.addBindValue(sessionId);

        if (!reviewQuery.exec()) {
            qDebug() << "Error saving session review:" << reviewQuery.lastError().text();
            QMessageBox::critical(this, "Database Error", "Failed to save session review: " + reviewQuery.lastError().text());
        } else {
            qDebug() << "Session Review Saved to DB:";
            qDebug() << "  Session Plan ID:" << currentSessionPlanId;
            qDebug() << "  Session ID:" << sessionId;
            qDebug() << "  Focus Score:" << currentFocusScore;
            qDebug() << "  Distractions:" << distractions;
            qDebug() << "  Effectiveness:" << effectiveness;
            qDebug() << "  Notes:" << notes;
            QMessageBox::information(this, "Session Review", "Session review saved successfully!");
        }
    } else {
        QMessageBox::information(this, "Session Review", "Session review cancelled.");
    }

    sessionReviewDialog->deleteLater();

    // Context menu for resource deletion
    resourceListReview->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(resourceListReview, &QListWidget::customContextMenuRequested, this, [this, resourceListReview, refreshResourceList](const QPoint &pos) {
        QListWidgetItem *item = resourceListReview->itemAt(pos);
        if (!item) return;
        QMenu menu;
        QAction *deleteAction = menu.addAction("Delete Resource");
        QAction *selectedAction = menu.exec(resourceListReview->viewport()->mapToGlobal(pos));
        if (selectedAction == deleteAction) {
            int resourceId = item->data(Qt::UserRole).toInt();
            QSqlQuery query(db);
            query.prepare("DELETE FROM session_resources WHERE id = ?");
            query.addBindValue(resourceId);
            query.exec();
            refreshResourceList();
        }
    });
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
