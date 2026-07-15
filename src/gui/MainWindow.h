#pragma once

#include "config.h"

#include <QMainWindow>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

class QAction;
class QCloseEvent;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QPaintEvent;
class QTableWidget;
class QTextEdit;

namespace mybarrier::gui {

class LayoutCanvas final : public QWidget {
public:
    explicit LayoutCanvas(QWidget* parent = nullptr);

    void setData(const DeviceConfig& device, std::vector<Peer> peers, std::vector<LayoutLink> links);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString labelForSide(const QString& side) const;

    DeviceConfig device_;
    std::vector<Peer> peers_;
    std::vector<LayoutLink> links_;
};

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    struct CommandResult {
        int exitCode = -1;
        QString output;
        QString error;
    };

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    QWidget* createLocalPanel();
    QWidget* createServerPage();
    QWidget* createClientPage();
    QWidget* createPeersPage();
    QWidget* createDiagnosticsPage();

    QString resolveCliPath() const;
    CommandResult runCli(const QStringList& args, int timeoutMs = 8000);
    void appendLog(const QString& text);
    void refreshAll();
    void refreshDevice(const DeviceConfig& device);
    void refreshPeers(const std::vector<Peer>& peers);
    void refreshCapabilities();
    void refreshLayout(const DeviceConfig& device, const std::vector<Peer>& peers);
    void setDaemonRunning(bool running);

    void saveDevice();
    void startDaemon();
    void stopDaemon();
    void discoverPeers();
    void pairPeer();
    void pingSelectedPeer();
    void sendClipboardToPeer();
    void saveLayoutLink();
    void clearLayout();
    void applyPeerPermissions();

    QString selectedPeerId() const;
    QString selectedPeerName() const;

    ConfigStore store_;
    QString cliPath_;
    QProcess daemon_;

    QAction* startAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QAction* refreshAction_ = nullptr;

    QLineEdit* nameEdit_ = nullptr;
    QComboBox* roleCombo_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    QLineEdit* daemonPairCodeEdit_ = nullptr;
    QLabel* deviceIdValue_ = nullptr;
    QLabel* configHomeValue_ = nullptr;
    QLabel* platformValue_ = nullptr;
    QLabel* daemonStatusValue_ = nullptr;

    LayoutCanvas* layoutCanvas_ = nullptr;
    QComboBox* sideCombo_ = nullptr;
    QComboBox* layoutPeerCombo_ = nullptr;

    QLineEdit* hostEdit_ = nullptr;
    QLineEdit* remotePortEdit_ = nullptr;
    QLineEdit* remoteCodeEdit_ = nullptr;

    QComboBox* peerCombo_ = nullptr;
    QLineEdit* clipboardEdit_ = nullptr;
    QTableWidget* peersTable_ = nullptr;
    QTableWidget* capabilitiesTable_ = nullptr;
    QTextEdit* logView_ = nullptr;
};

}  // namespace mybarrier::gui
