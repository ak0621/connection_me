#include "MainWindow.h"

#include "platform.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <utility>

namespace mybarrier::gui {
namespace {

QString toQString(const std::string& value) {
    return QString::fromStdString(value);
}

std::string toStdString(const QString& value) {
    return value.trimmed().toStdString();
}

QString peerDisplayName(const Peer& peer) {
    const QString name = toQString(peer.name);
    if (!name.isEmpty()) {
        return name;
    }
    return toQString(peer.id).left(12);
}

QTableWidgetItem* readOnlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QTableWidgetItem* checkItem(bool checked) {
    auto* item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QString checkedArg(const QTableWidgetItem* item) {
    return item != nullptr && item->checkState() == Qt::Checked ? QStringLiteral("1") : QStringLiteral("0");
}

QString runtimePlatformName() {
#ifdef Q_OS_WIN
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return QStringLiteral("macOS");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Linux");
#else
    return QStringLiteral("Unknown platform");
#endif
}

QString quoteForLog(QString value) {
    if (value.isEmpty()) {
        return QStringLiteral("''");
    }
    const bool needsQuotes = value.contains(QLatin1Char(' ')) || value.contains(QLatin1Char('\t')) ||
                             value.contains(QLatin1Char('"'));
    if (!needsQuotes) {
        return value;
    }
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(value);
}

QString formatCommandForLog(const QString& program, const QStringList& args) {
    QStringList parts;
    parts << quoteForLog(program);
    for (const QString& arg : args) {
        parts << quoteForLog(arg);
    }
    return parts.join(QStringLiteral(" "));
}

QString commandOutput(const MainWindow::CommandResult& result) {
    QStringList parts;
    parts << QStringLiteral("exit=%1").arg(result.exitCode);
    if (!result.output.trimmed().isEmpty()) {
        parts << QStringLiteral("stdout:\n%1").arg(result.output.trimmed());
    }
    if (!result.error.trimmed().isEmpty()) {
        parts << QStringLiteral("stderr:\n%1").arg(result.error.trimmed());
    }
    return parts.join(QStringLiteral("\n"));
}

}  // namespace

LayoutCanvas::LayoutCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LayoutCanvas::setData(const DeviceConfig& device, std::vector<Peer> peers, std::vector<LayoutLink> links) {
    device_ = device;
    peers_ = std::move(peers);
    links_ = std::move(links);
    update();
}

QSize LayoutCanvas::sizeHint() const {
    return QSize(520, 320);
}

QString LayoutCanvas::labelForSide(const QString& side) const {
    const QString localName = toQString(device_.name);
    const QString localId = toQString(device_.id);
    for (const auto& link : links_) {
        const QString source = toQString(link.source);
        if ((source == localName || source == localId) && toQString(link.side) == side) {
            const QString target = toQString(link.target);
            auto match = std::find_if(peers_.begin(), peers_.end(), [&](const Peer& peer) {
                return toQString(peer.id) == target || toQString(peer.name) == target;
            });
            if (match != peers_.end()) {
                return peerDisplayName(*match);
            }
            return target;
        }
    }
    return {};
}

void LayoutCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(247, 249, 252));

    const QRectF area = QRectF(rect()).adjusted(24, 22, -24, -22);
    const qreal gap = 14.0;
    const qreal cellWidth = (area.width() - gap * 2.0) / 3.0;
    const qreal cellHeight = (area.height() - gap * 2.0) / 3.0;

    auto cell = [&](int column, int row) {
        return QRectF(area.left() + column * (cellWidth + gap), area.top() + row * (cellHeight + gap), cellWidth, cellHeight);
    };

    auto drawScreen = [&](const QRectF& bounds, const QString& title, const QString& subtitle, const QColor& fill, const QColor& stroke) {
        painter.setPen(QPen(stroke, 1.4));
        painter.setBrush(fill);
        painter.drawRoundedRect(bounds, 7.0, 7.0);
        painter.setPen(QColor(35, 45, 62));
        QFont titleFont = font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(bounds.adjusted(10, 10, -10, -bounds.height() / 2.0), Qt::AlignCenter, title);
        painter.setPen(QColor(92, 103, 119));
        painter.setFont(font());
        painter.drawText(bounds.adjusted(10, bounds.height() / 2.0 - 6.0, -10, -10), Qt::AlignCenter | Qt::TextWordWrap, subtitle);
    };

    const QRectF center = cell(1, 1);
    drawScreen(center, toQString(device_.name), QStringLiteral("local %1").arg(toQString(device_.role)), QColor(218, 235, 255), QColor(74, 130, 198));

    const std::map<QString, std::pair<int, int>> positions = {
        {QStringLiteral("left"), {0, 1}},
        {QStringLiteral("right"), {2, 1}},
        {QStringLiteral("top"), {1, 0}},
        {QStringLiteral("bottom"), {1, 2}},
    };

    for (const auto& entry : positions) {
        const QString label = labelForSide(entry.first);
        const QRectF target = cell(entry.second.first, entry.second.second);
        if (label.isEmpty()) {
            drawScreen(target, entry.first.toUpper(), QStringLiteral("not assigned"), QColor(255, 255, 255), QColor(197, 205, 216));
        } else {
            drawScreen(target, label, entry.first, QColor(230, 247, 239), QColor(68, 153, 110));
            painter.setPen(QPen(QColor(118, 128, 141), 1.2));
            painter.drawLine(center.center(), target.center());
        }
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), cliPath_(resolveCliPath()) {
    buildUi();

    connect(&daemon_, &QProcess::readyReadStandardOutput, this, [this]() {
        appendLog(QString::fromLocal8Bit(daemon_.readAllStandardOutput()).trimmed());
    });
    connect(&daemon_, &QProcess::readyReadStandardError, this, [this]() {
        appendLog(QString::fromLocal8Bit(daemon_.readAllStandardError()).trimmed());
    });
    connect(&daemon_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        setDaemonRunning(false);
        appendLog(QStringLiteral("daemon stopped exit=%1").arg(exitCode));
    });

    appendLog(QStringLiteral("MyBarrier GUI started on %1").arg(runtimePlatformName()));
    refreshAll();
    appendLog(QStringLiteral("Config home: %1").arg(QString::fromStdString(store_.home().string())));
    appendLog(cliPath_.isEmpty() ? QStringLiteral("CLI executable was not found") : QStringLiteral("CLI executable: %1").arg(cliPath_));
}

MainWindow::~MainWindow() {
    stopDaemon();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    stopDaemon();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("MyBarrier"));

    auto* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    startAction_ = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Start"));
    stopAction_ = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("Stop"));
    refreshAction_ = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QStringLiteral("Refresh"));
    auto* discoverAction = toolbar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("Discover"));

    connect(startAction_, &QAction::triggered, this, [this]() { startDaemon(); });
    connect(stopAction_, &QAction::triggered, this, [this]() { stopDaemon(); });
    connect(refreshAction_, &QAction::triggered, this, [this]() { refreshAll(); });
    connect(discoverAction, &QAction::triggered, this, [this]() { discoverPeers(); });

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(10);

    root->addWidget(createLocalPanel());

    auto* tabs = new QTabWidget(central);
    tabs->addTab(createServerPage(), QStringLiteral("Server"));
    tabs->addTab(createClientPage(), QStringLiteral("Client"));
    tabs->addTab(createPeersPage(), QStringLiteral("Peers"));
    tabs->addTab(createDiagnosticsPage(), QStringLiteral("Diagnostics"));
    root->addWidget(tabs, 1);

    setCentralWidget(central);
    createLogDock();
    if (logDock_ != nullptr) {
        QAction* logAction = logDock_->toggleViewAction();
        logAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        toolbar->addAction(logAction);
    }
    statusBar()->showMessage(QStringLiteral("Ready"));
    setDaemonRunning(false);
}

QWidget* MainWindow::createLocalPanel() {
    auto* box = new QGroupBox(QStringLiteral("Local device"), this);
    auto* grid = new QGridLayout(box);

    nameEdit_ = new QLineEdit(box);
    roleCombo_ = new QComboBox(box);
    roleCombo_->addItems({QStringLiteral("server"), QStringLiteral("client"), QStringLiteral("peer")});
    portEdit_ = new QLineEdit(box);
    portEdit_->setMaximumWidth(110);
    daemonPairCodeEdit_ = new QLineEdit(box);
    daemonPairCodeEdit_->setMaximumWidth(140);
    daemonPairCodeEdit_->setPlaceholderText(QStringLiteral("pair code"));

    deviceIdValue_ = new QLabel(box);
    configHomeValue_ = new QLabel(box);
    platformValue_ = new QLabel(box);
    daemonStatusValue_ = new QLabel(box);
    deviceIdValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    configHomeValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* saveButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Save"), box);
    connect(saveButton, &QPushButton::clicked, this, [this]() { saveDevice(); });

    grid->addWidget(new QLabel(QStringLiteral("Name"), box), 0, 0);
    grid->addWidget(nameEdit_, 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Role"), box), 0, 2);
    grid->addWidget(roleCombo_, 0, 3);
    grid->addWidget(new QLabel(QStringLiteral("Port"), box), 0, 4);
    grid->addWidget(portEdit_, 0, 5);
    grid->addWidget(new QLabel(QStringLiteral("Pair code"), box), 0, 6);
    grid->addWidget(daemonPairCodeEdit_, 0, 7);
    grid->addWidget(saveButton, 0, 8);

    grid->addWidget(new QLabel(QStringLiteral("Device ID"), box), 1, 0);
    grid->addWidget(deviceIdValue_, 1, 1, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("Platform"), box), 1, 3);
    grid->addWidget(platformValue_, 1, 4);
    grid->addWidget(new QLabel(QStringLiteral("Daemon"), box), 1, 5);
    grid->addWidget(daemonStatusValue_, 1, 6);
    grid->addWidget(new QLabel(QStringLiteral("Config"), box), 2, 0);
    grid->addWidget(configHomeValue_, 2, 1, 1, 8);

    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(8, 0);
    return box;
}

QWidget* MainWindow::createServerPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    layoutCanvas_ = new LayoutCanvas(page);
    layout->addWidget(layoutCanvas_, 1);

    auto* controls = new QHBoxLayout();
    sideCombo_ = new QComboBox(page);
    sideCombo_->addItems({QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("top"), QStringLiteral("bottom")});
    layoutPeerCombo_ = new QComboBox(page);
    auto* linkButton = new QPushButton(style()->standardIcon(QStyle::SP_ArrowRight), QStringLiteral("Link"), page);
    auto* clearButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogResetButton), QStringLiteral("Clear"), page);
    connect(linkButton, &QPushButton::clicked, this, [this]() { saveLayoutLink(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() { clearLayout(); });

    controls->addWidget(new QLabel(QStringLiteral("Side"), page));
    controls->addWidget(sideCombo_);
    controls->addWidget(new QLabel(QStringLiteral("Target"), page));
    controls->addWidget(layoutPeerCombo_, 1);
    controls->addWidget(linkButton);
    controls->addWidget(clearButton);
    layout->addLayout(controls);
    return page;
}

QWidget* MainWindow::createClientPage() {
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);

    auto* formBox = new QGroupBox(QStringLiteral("Pairing"), page);
    auto* form = new QFormLayout(formBox);
    hostEdit_ = new QLineEdit(formBox);
    remotePortEdit_ = new QLineEdit(QStringLiteral("37373"), formBox);
    remoteCodeEdit_ = new QLineEdit(formBox);
    remoteCodeEdit_->setPlaceholderText(QStringLiteral("pair code"));
    form->addRow(QStringLiteral("Host"), hostEdit_);
    form->addRow(QStringLiteral("Port"), remotePortEdit_);
    form->addRow(QStringLiteral("Code"), remoteCodeEdit_);

    auto* buttons = new QHBoxLayout();
    auto* pairButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogApplyButton), QStringLiteral("Pair"), formBox);
    auto* discoverButton = new QPushButton(style()->standardIcon(QStyle::SP_BrowserReload), QStringLiteral("Discover"), formBox);
    connect(pairButton, &QPushButton::clicked, this, [this]() { pairPeer(); });
    connect(discoverButton, &QPushButton::clicked, this, [this]() { discoverPeers(); });
    buttons->addStretch(1);
    buttons->addWidget(discoverButton);
    buttons->addWidget(pairButton);
    form->addRow(buttons);

    outer->addWidget(formBox);
    outer->addStretch(1);
    return page;
}

QWidget* MainWindow::createPeersPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    peersTable_ = new QTableWidget(page);
    peersTable_->setColumnCount(7);
    peersTable_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Name"), QStringLiteral("Host"), QStringLiteral("Port"), QStringLiteral("Clipboard"), QStringLiteral("Files"), QStringLiteral("Input")});
    peersTable_->horizontalHeader()->setStretchLastSection(true);
    peersTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    peersTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    peersTable_->setColumnHidden(0, true);
    layout->addWidget(peersTable_, 1);

    auto* controls = new QHBoxLayout();
    peerCombo_ = new QComboBox(page);
    clipboardEdit_ = new QLineEdit(page);
    clipboardEdit_->setPlaceholderText(QStringLiteral("clipboard text"));
    auto* pingButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogYesButton), QStringLiteral("Ping"), page);
    auto* sendButton = new QPushButton(style()->standardIcon(QStyle::SP_ArrowRight), QStringLiteral("Send Clipboard"), page);
    auto* applyButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Apply Permissions"), page);
    connect(pingButton, &QPushButton::clicked, this, [this]() { pingSelectedPeer(); });
    connect(sendButton, &QPushButton::clicked, this, [this]() { sendClipboardToPeer(); });
    connect(applyButton, &QPushButton::clicked, this, [this]() { applyPeerPermissions(); });

    controls->addWidget(peerCombo_, 1);
    controls->addWidget(pingButton);
    controls->addWidget(clipboardEdit_, 2);
    controls->addWidget(sendButton);
    controls->addWidget(applyButton);
    layout->addLayout(controls);
    return page;
}

QWidget* MainWindow::createDiagnosticsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    capabilitiesTable_ = new QTableWidget(page);
    capabilitiesTable_->setColumnCount(3);
    capabilitiesTable_->setHorizontalHeaderLabels({QStringLiteral("Capability"), QStringLiteral("Status"), QStringLiteral("Detail")});
    capabilitiesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    capabilitiesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    capabilitiesTable_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(capabilitiesTable_);
    return page;
}

void MainWindow::createLogDock() {
    logDock_ = new QDockWidget(QStringLiteral("Run Log"), this);
    logDock_->setObjectName(QStringLiteral("RunLogDock"));
    logDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    logDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto* panel = new QWidget(logDock_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* controls = new QHBoxLayout();
    auto* platformLabel = new QLabel(QStringLiteral("%1 runtime").arg(runtimePlatformName()), panel);
    auto* clearButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogResetButton), QStringLiteral("Clear"), panel);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (logView_ != nullptr) {
            logView_->clear();
        }
    });
    controls->addWidget(platformLabel);
    controls->addStretch(1);
    controls->addWidget(clearButton);
    layout->addLayout(controls);

    logView_ = new QTextEdit(panel);
    logView_->setReadOnly(true);
    logView_->setMinimumHeight(180);
    logView_->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(logView_);

    logDock_->setWidget(panel);
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    logDock_->show();
}

QString MainWindow::resolveCliPath() const {
    const QString envPath = qEnvironmentVariable("MYBARRIER_CLI");
    if (!envPath.isEmpty() && QFileInfo(envPath).isExecutable()) {
        return QFileInfo(envPath).absoluteFilePath();
    }

#ifdef Q_OS_WIN
    const QString executableName = QStringLiteral("mybarrier.exe");
#else
    const QString executableName = QStringLiteral("mybarrier");
#endif

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        appDir.filePath(executableName),
        QDir::cleanPath(appDir.filePath(QStringLiteral("../bin/") + executableName)),
        QDir::cleanPath(appDir.filePath(QStringLiteral("../../../") + executableName)),
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

MainWindow::CommandResult MainWindow::runCli(const QStringList& args, int timeoutMs) {
    if (cliPath_.isEmpty()) {
        CommandResult result{-1, {}, QStringLiteral("mybarrier CLI executable was not found")};
        appendLog(commandOutput(result));
        return result;
    }

    appendLog(QStringLiteral("command: %1").arg(formatCommandForLog(cliPath_, args)));

    QProcess process;
    process.setProgram(cliPath_);
    process.setArguments(args);
    process.start();
    if (!process.waitForStarted(1500)) {
        CommandResult result{-1, {}, process.errorString()};
        appendLog(commandOutput(result));
        return result;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        CommandResult result{-1, QString::fromLocal8Bit(process.readAllStandardOutput()), QStringLiteral("command timed out")};
        appendLog(commandOutput(result));
        return result;
    }
    CommandResult result{process.exitCode(), QString::fromLocal8Bit(process.readAllStandardOutput()), QString::fromLocal8Bit(process.readAllStandardError())};
    appendLog(commandOutput(result));
    return result;
}

void MainWindow::appendLog(const QString& text) {
    if (logView_ == nullptr) {
        return;
    }
    QString normalized = text.trimmed();
    if (normalized.isEmpty()) {
        return;
    }
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar::CarriageReturn, QChar::LineFeed);
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QStringList lines = normalized.split(QChar::LineFeed, Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        logView_->append(QStringLiteral("[%1] %2").arg(timestamp, line.trimmed()));
    }
}

void MainWindow::refreshAll() {
    try {
        const DeviceConfig device = store_.load_device();
        const std::vector<Peer> peers = store_.load_peers();
        refreshDevice(device);
        refreshPeers(peers);
        refreshCapabilities();
        refreshLayout(device, peers);
        statusBar()->showMessage(QStringLiteral("Refreshed"), 2500);
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, QStringLiteral("MyBarrier"), QString::fromLocal8Bit(ex.what()));
    }
}

void MainWindow::refreshDevice(const DeviceConfig& device) {
    nameEdit_->setText(toQString(device.name));
    roleCombo_->setCurrentText(toQString(device.role));
    portEdit_->setText(QString::number(device.port));
    deviceIdValue_->setText(toQString(device.id));
    configHomeValue_->setText(QString::fromStdString(store_.home().string()));
    platformValue_->setText(QString::fromStdString(platform_name()));
}

void MainWindow::refreshPeers(const std::vector<Peer>& peers) {
    peerCombo_->clear();
    layoutPeerCombo_->clear();
    peersTable_->setRowCount(static_cast<int>(peers.size()));

    for (int row = 0; row < static_cast<int>(peers.size()); ++row) {
        const Peer& peer = peers[static_cast<std::size_t>(row)];
        const QString id = toQString(peer.id);
        const QString label = peerDisplayName(peer);
        peerCombo_->addItem(label, id);
        layoutPeerCombo_->addItem(label, label);

        peersTable_->setItem(row, 0, readOnlyItem(id));
        peersTable_->setItem(row, 1, readOnlyItem(label));
        peersTable_->setItem(row, 2, readOnlyItem(toQString(peer.host)));
        peersTable_->setItem(row, 3, readOnlyItem(QString::number(peer.port)));
        peersTable_->setItem(row, 4, checkItem(peer.allow_clipboard));
        peersTable_->setItem(row, 5, checkItem(peer.allow_files));
        peersTable_->setItem(row, 6, checkItem(peer.allow_input));
    }
}

void MainWindow::refreshCapabilities() {
    const auto capabilities = detect_platform_capabilities();
    capabilitiesTable_->setRowCount(static_cast<int>(capabilities.size()));
    for (int row = 0; row < static_cast<int>(capabilities.size()); ++row) {
        const auto& cap = capabilities[static_cast<std::size_t>(row)];
        capabilitiesTable_->setItem(row, 0, readOnlyItem(toQString(cap.name)));
        capabilitiesTable_->setItem(row, 1, readOnlyItem(toQString(cap.status)));
        capabilitiesTable_->setItem(row, 2, readOnlyItem(toQString(cap.detail)));
    }
}

void MainWindow::refreshLayout(const DeviceConfig& device, const std::vector<Peer>& peers) {
    layoutCanvas_->setData(device, peers, store_.load_layout());
}

void MainWindow::setDaemonRunning(bool running) {
    if (daemonStatusValue_ != nullptr) {
        daemonStatusValue_->setText(running ? QStringLiteral("running") : QStringLiteral("stopped"));
    }
    if (startAction_ != nullptr) {
        startAction_->setEnabled(!running);
    }
    if (stopAction_ != nullptr) {
        stopAction_->setEnabled(running);
    }
}

void MainWindow::saveDevice() {
    try {
        const uint16_t port = static_cast<uint16_t>(portEdit_->text().trimmed().toUShort());
        store_.load_device(toStdString(nameEdit_->text()), port, toStdString(roleCombo_->currentText()));
        appendLog(QStringLiteral("device saved"));
        refreshAll();
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, QStringLiteral("MyBarrier"), QString::fromLocal8Bit(ex.what()));
    }
}

void MainWindow::startDaemon() {
    if (daemon_.state() != QProcess::NotRunning) {
        return;
    }
    saveDevice();
    if (cliPath_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("MyBarrier"), QStringLiteral("mybarrier CLI executable was not found"));
        return;
    }

    QStringList args = {
        QStringLiteral("serve"),
        QStringLiteral("--name"), nameEdit_->text().trimmed(),
        QStringLiteral("--port"), portEdit_->text().trimmed(),
        QStringLiteral("--role"), roleCombo_->currentText().trimmed(),
    };
    const QString pairCode = daemonPairCodeEdit_->text().trimmed();
    if (!pairCode.isEmpty()) {
        args << QStringLiteral("--pair-code") << pairCode;
    }

    daemon_.setProgram(cliPath_);
    daemon_.setArguments(args);
    appendLog(QStringLiteral("daemon command: %1").arg(formatCommandForLog(cliPath_, args)));
    daemon_.start();
    if (!daemon_.waitForStarted(1800)) {
        appendLog(QStringLiteral("daemon failed to start: %1").arg(daemon_.errorString()));
        QMessageBox::warning(this, QStringLiteral("MyBarrier"), daemon_.errorString());
        return;
    }
    setDaemonRunning(true);
    appendLog(QStringLiteral("daemon started pid=%1").arg(daemon_.processId()));
}

void MainWindow::stopDaemon() {
    if (daemon_.state() == QProcess::NotRunning) {
        setDaemonRunning(false);
        return;
    }
    appendLog(QStringLiteral("stopping daemon"));
    daemon_.terminate();
    if (!daemon_.waitForFinished(2500)) {
        daemon_.kill();
        daemon_.waitForFinished(1000);
    }
    setDaemonRunning(false);
}

void MainWindow::discoverPeers() {
    runCli({QStringLiteral("discover"), QStringLiteral("--timeout"), QStringLiteral("1500")}, 3000);
}

void MainWindow::pairPeer() {
    const QString host = hostEdit_->text().trimmed();
    const QString port = remotePortEdit_->text().trimmed();
    const QString code = remoteCodeEdit_->text().trimmed();
    if (host.isEmpty() || code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("MyBarrier"), QStringLiteral("Host and code are required"));
        return;
    }
    runCli({QStringLiteral("pair"), QStringLiteral("--host"), host, QStringLiteral("--port"), port, QStringLiteral("--code"), code}, 8000);
    refreshAll();
}

void MainWindow::pingSelectedPeer() {
    const QString peerId = selectedPeerId();
    if (peerId.isEmpty()) {
        return;
    }
    runCli({QStringLiteral("ping"), QStringLiteral("--peer"), peerId}, 5000);
}

void MainWindow::sendClipboardToPeer() {
    const QString peerId = selectedPeerId();
    if (peerId.isEmpty()) {
        return;
    }
    runCli({QStringLiteral("clipboard-send"), QStringLiteral("--peer"), peerId, QStringLiteral("--text"), clipboardEdit_->text()}, 8000);
}

void MainWindow::saveLayoutLink() {
    const QString target = layoutPeerCombo_->currentData().toString();
    if (target.isEmpty()) {
        return;
    }
    runCli({QStringLiteral("layout-add"), QStringLiteral("--from"), nameEdit_->text().trimmed(), QStringLiteral("--side"), sideCombo_->currentText(), QStringLiteral("--to"), target}, 3000);
    refreshAll();
}

void MainWindow::clearLayout() {
    runCli({QStringLiteral("layout-clear")}, 3000);
    refreshAll();
}

void MainWindow::applyPeerPermissions() {
    for (int row = 0; row < peersTable_->rowCount(); ++row) {
        const QTableWidgetItem* idItem = peersTable_->item(row, 0);
        if (idItem == nullptr || idItem->text().isEmpty()) {
            continue;
        }
        runCli({
            QStringLiteral("peer-permit"),
            QStringLiteral("--peer"), idItem->text(),
            QStringLiteral("--clipboard"), checkedArg(peersTable_->item(row, 4)),
            QStringLiteral("--files"), checkedArg(peersTable_->item(row, 5)),
            QStringLiteral("--input"), checkedArg(peersTable_->item(row, 6)),
        }, 3000);
    }
    refreshAll();
}

QString MainWindow::selectedPeerId() const {
    return peerCombo_ == nullptr ? QString() : peerCombo_->currentData().toString();
}

QString MainWindow::selectedPeerName() const {
    return peerCombo_ == nullptr ? QString() : peerCombo_->currentText();
}

}  // namespace mybarrier::gui
