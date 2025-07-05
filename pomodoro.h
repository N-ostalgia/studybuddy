#ifndef POMODORO_H
#define POMODORO_H

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QString>

class PomodoroTimer : public QObject
{
    Q_OBJECT

public:
    explicit PomodoroTimer(QObject *parent = nullptr);
    ~PomodoroTimer();

    // Configuration
    void setStudyDuration(int minutes);
    void setBreakDuration(int minutes);
    void setLongBreakDuration(int minutes);
    void setCyclesBeforeLongBreak(int cycles);

    // Control
    void start();
    void pause();
    void reset();
    void skip();

    // Status
    bool isRunning() const;
    bool isStudyTime() const;
    int currentCycle() const;
    QString timeRemaining() const;
    int totalCycles() const;
    int getStudyDuration() const;

signals:
    void timeUpdated(const QString &time);
    void cycleChanged(int cycle);
    void modeChanged(bool isStudyTime);
    void timerCompleted();
    void breakStarted();
    void studyStarted();

private slots:
    void updateTimer();
    void switchMode();

private:
    QTimer *timer;
    QTime remainingTime;
    int studyMinutes;
    int breakMinutes;
    int longBreakMinutes;
    int cyclesBeforeLongBreak;
    int currentCycleCount;
    bool isStudyMode;
    bool isPaused;
    int totalCyclesCompleted;

    void initializeTimer();
    void updateDisplay();
};

#endif // POMODORO_H 