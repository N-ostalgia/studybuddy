#include "pomodoro.h"
#include <QDebug>

PomodoroTimer::PomodoroTimer(QObject *parent)
    : QObject(parent)
    , timer(new QTimer(this))
    , studyMinutes(25)
    , breakMinutes(5)
    , longBreakMinutes(15)
    , cyclesBeforeLongBreak(4)
    , currentCycleCount(0)
    , isStudyMode(true)
    , isPaused(false)
    , totalCyclesCompleted(0)
{
    initializeTimer();
}

PomodoroTimer::~PomodoroTimer()
{
    if (timer->isActive()) {
        timer->stop();
    }
}

void PomodoroTimer::initializeTimer()
{
    connect(timer, &QTimer::timeout, this, &PomodoroTimer::updateTimer);
    remainingTime = QTime(0, studyMinutes);
    updateDisplay();
}

void PomodoroTimer::setStudyDuration(int minutes)
{
    studyMinutes = minutes;
    if (isStudyMode && !isRunning()) {
        remainingTime = QTime(0, minutes);
        updateDisplay();
    }
}

void PomodoroTimer::setBreakDuration(int minutes)
{
    breakMinutes = minutes;
    if (!isStudyMode && !isRunning()) {
        remainingTime = QTime(0, minutes);
        updateDisplay();
    }
}

void PomodoroTimer::setLongBreakDuration(int minutes)
{
    longBreakMinutes = minutes;
}

void PomodoroTimer::setCyclesBeforeLongBreak(int cycles)
{
    cyclesBeforeLongBreak = cycles;
}

void PomodoroTimer::start()
{
    if (!isPaused) {
        remainingTime = QTime(0, isStudyMode ? studyMinutes : breakMinutes);
    }
    timer->start(1000); // Update every second
    isPaused = false;
}

void PomodoroTimer::pause()
{
    timer->stop();
    isPaused = true;
}

void PomodoroTimer::reset()
{
    timer->stop();
    isPaused = false;
    remainingTime = QTime(0, isStudyMode ? studyMinutes : breakMinutes);
    updateDisplay();
}

void PomodoroTimer::skip()
{
    switchMode();
}

bool PomodoroTimer::isRunning() const
{
    return timer->isActive();
}

bool PomodoroTimer::isStudyTime() const
{
    return isStudyMode;
}

int PomodoroTimer::currentCycle() const
{
    return currentCycleCount;
}

QString PomodoroTimer::timeRemaining() const
{
    return remainingTime.toString("mm:ss");
}

int PomodoroTimer::totalCycles() const
{
    return totalCyclesCompleted;
}

int PomodoroTimer::getStudyDuration() const
{
    return studyMinutes;
}

void PomodoroTimer::updateTimer()
{
    remainingTime = remainingTime.addSecs(-1);
    
    if (remainingTime == QTime(0, 0)) {
        timer->stop();
        emit timerCompleted();
        switchMode();
    }
    
    updateDisplay();
}

void PomodoroTimer::switchMode()
{
    isStudyMode = !isStudyMode;
    
    if (isStudyMode) {
        currentCycleCount++;
        if (currentCycleCount % cyclesBeforeLongBreak == 0) {
            remainingTime = QTime(0, longBreakMinutes);
        } else {
            remainingTime = QTime(0, breakMinutes);
        }
        emit studyStarted();
    } else {
        remainingTime = QTime(0, studyMinutes);
        totalCyclesCompleted++;
        emit breakStarted();
    }
    
    emit modeChanged(isStudyMode);
    emit cycleChanged(currentCycleCount);
    updateDisplay();
}

void PomodoroTimer::updateDisplay()
{
    emit timeUpdated(remainingTime.toString("mm:ss"));
} 