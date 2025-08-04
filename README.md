# StudyBuddy - AI-Powered Study Assistant

A comprehensive study productivity application built with Qt/C++ and OpenCV, featuring real-time focus tracking, AI-powered study assistance, and smart planning capabilities.

## Features

- **Real-time Focus Tracking**: Computer vision-based attention monitoring using OpenCV
- **AI Study Assistant**: Powered by Google Gemini API for personalized study tips
- **Smart Calendar Planner**: Automated study session scheduling based on free time
- **Advanced Analytics**: Focus patterns, productivity insights, and progress tracking
- **Pomodoro Timer**: Customizable study/break cycles with notifications
- **Achievement System**: Gamified learning with milestones and streaks
- **Survey System**: Session reflection and distraction tracking
- **Cross-platform**: Works on Windows, macOS, and Linux

## Setup Instructions

### Prerequisites
- Qt 6.9.0 or later
- OpenCV 4.x
- C++ compiler (MSVC, GCC, or Clang)

### API Key Configuration

1. **Create `config.ini` file** in the project root:
```ini
[API]
GEMINI_API_KEY=your_google_gemini_api_key_here
```

2. **Get a Google Gemini API Key**:
   - Visit [Google AI Studio](https://makersuite.google.com/app/apikey)
   - Create a new API key
   - Add it to your `config.ini` file

3. **Security Note**: The `config.ini` file is already added to `.gitignore` to prevent accidental commits of your API key.

### Building the Project

1. Open `STUDYBUDDY.pro` in Qt Creator
2. Configure your build settings
3. Build and run the project

## Project Structure

- `mainwindow.cpp/h`: Main application window and UI logic
- `studygoals.cpp/h`: Goal management and tracking
- `analytics.cpp/h`: Data analysis and chart generation
- `achievements.cpp/h`: Achievement system and gamification
- `survey.cpp/h`: Session reflection and feedback system
- `streak.cpp/h`: Study streak tracking
- `pomodoro.cpp/h`: Timer functionality

## Database

The application uses a unified SQLite database (`studybuddy.db`) that stores:
- Study sessions and focus data
- Goals and progress tracking
- Achievements and streaks
- Survey responses
- Calendar and planning data

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request
