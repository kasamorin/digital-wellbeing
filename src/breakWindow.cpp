#include "breakWindow.h"
#include "i18n.h"

#include <cstdlib>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class BreakWindow : public QWidget
{
public:
    BreakWindow(int totalSeconds, QEventLoop *loop);
    bool wasSkipped() const { return mSkipped; }

private:
    void tick();
    void skipBreak();

    QLabel *mLabel;
    QPushButton *mSkipButton;
    QTimer *mTimer;
    QEventLoop *mLoop;
    int mRemaining;
    bool mSkipped;
    bool mNotifiedReminder;
};

BreakWindow::BreakWindow(int totalSeconds, QEventLoop *loop)
    : QWidget(nullptr, Qt::WindowStaysOnTopHint)
    , mLoop(loop)
    , mRemaining(totalSeconds)
    , mSkipped(false)
    , mNotifiedReminder(false)
{
    setWindowTitle(_("Take a break"));
    showFullScreen();
    setStyleSheet("background-color: rgba(24, 24, 27, 0.92);");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    mLabel = new QLabel(this);
    mLabel->setAlignment(Qt::AlignCenter);
    mLabel->setStyleSheet(
        "font-size: 96pt; font-weight: bold; color: #f4f4f5; "
        "background: transparent;");
    layout->addWidget(mLabel);

    layout->addSpacing(40);

    mSkipButton = new QPushButton(_("Skip Break"), this);
    mSkipButton->setMinimumSize(220, 60);
    mSkipButton->setStyleSheet(
        "font-size: 18pt; padding: 12px 32px; "
        "background-color: rgba(244, 244, 245, 0.15); "
        "color: #f4f4f5; border: 1px solid rgba(244, 244, 245, 0.3); "
        "border-radius: 8px;");
    connect(mSkipButton, &QPushButton::clicked,
        this, &BreakWindow::skipBreak);
    layout->addWidget(mSkipButton, 0, Qt::AlignCenter);

    QShortcut *quitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(quitShortcut, &QShortcut::activated,
        this, &BreakWindow::skipBreak);

    mTimer = new QTimer(this);
    connect(mTimer, &QTimer::timeout, this, &BreakWindow::tick);
    mTimer->start(1000);

    tick();  // show initial time immediately
}

void BreakWindow::tick()
{
    int mins = mRemaining / 60;
    int secs = mRemaining % 60;
    mLabel->setText(QString::asprintf("%d:%02d", mins, secs));

    if (!mNotifiedReminder && mRemaining <= 300)
    {
        mNotifiedReminder = true;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "notify-send -t 30000 'Digital Wellbeing' '%s'",
            _("5 minutes of break remaining"));
        int ret = system(cmd);
        (void)ret;
    }

    if (mRemaining <= 0)
    {
        close();  // timer expired — mSkipped stays false
        mLoop->quit();
        return;
    }
    mRemaining--;
}

void BreakWindow::skipBreak()
{
    mSkipped = true;
    close();
    mLoop->quit();
}

/* ---- extern "C" entry point ---- */
bool breakWindowShow(int breakSeconds)
{
    QEventLoop loop;

    BreakWindow w(breakSeconds, &loop);
    w.show();

    /* Block until w calls loop.quit() (timer expired or user skip).
     * The window closes but the QApplication keeps running for the
     * next break cycle. */
    loop.exec();

    return w.wasSkipped();
}