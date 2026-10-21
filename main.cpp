// Copyright (C) 2026 guantamanera <guantamanera@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://gnu.org>.
#include "devices.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

class MainWindow final : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle("USB/IP Manager");
        resize(1000, 640);
        auto *central = new QWidget;
        auto *layout = new QVBoxLayout(central);
        auto *title = new QLabel("<h2>USB/IP Server</h2>");
        layout->addWidget(title);
        serverStatus = new QLabel("Checking server...");
        layout->addWidget(serverStatus);
        auto *warning = new QLabel(
            "USB/IP has no built-in authentication or encryption. Use a trusted network or VPN, "
            "and restrict TCP port 3240 in your firewall. Sharing disconnects a device from local apps.");
        warning->setWordWrap(true);
        layout->addWidget(warning);

        auto *serverButtons = new QHBoxLayout;
        start = new QPushButton("Start server");
        stop = new QPushButton("Stop server");
        auto *refresh = new QPushButton("Refresh");
        serverButtons->addWidget(start);
        serverButtons->addWidget(stop);
        serverButtons->addStretch();
        serverButtons->addWidget(refresh);
        layout->addLayout(serverButtons);

        table = new QTableWidget(0, 5);
        table->setHorizontalHeaderLabels({"Bus ID", "Device", "Vendor:Product", "Driver", "Status"});
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->verticalHeader()->hide();
        layout->addWidget(table, 1);

        auto *deviceButtons = new QHBoxLayout;
        share = new QPushButton("Share selected device");
        unshare = new QPushButton("Unshare selected device");
        deviceButtons->addWidget(share);
        deviceButtons->addWidget(unshare);
        deviceButtons->addStretch();
        layout->addLayout(deviceButtons);
        hint = new QLabel;
        hint->setWordWrap(true);
        hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(hint);
        log = new QTextEdit;
        log->setReadOnly(true);
        log->setMaximumHeight(115);
        log->setPlaceholderText("Operation results appear here.");
        layout->addWidget(log);
        setCentralWidget(central);

        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); checkServer(); });
        connect(table, &QTableWidget::itemSelectionChanged, this, [this] { updateControls(); });
        connect(start, &QPushButton::clicked, this, [this] {
            if (QMessageBox::warning(this, "Start USB/IP server",
                    "The server listens on TCP port 3240. Any host that can reach it may attach "
                    "to shared devices. No firewall rules will be changed.\n\nStart the server?",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                operate({"start"});
        });
        connect(stop, &QPushButton::clicked, this, [this] {
            if (QMessageBox::warning(this, "Stop USB/IP server",
                    "Stopping the listener prevents new connections but may not disconnect existing "
                    "clients. Unshare a device to revoke access to it.\n\nStop the server?",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                operate({"stop"});
        });
        connect(share, &QPushButton::clicked, this, [this] { changeSharing(true); });
        connect(unshare, &QPushButton::clicked, this, [this] { changeSharing(false); });
        connect(&timer, &QTimer::timeout, this, [this] {
            if (!busy) {
                refreshDevices();
                checkServer();
            }
        });
        timer.start(3000);
        refreshDevices();
        checkServer();
    }

private:
    void refreshDevices()
    {
        const QString selected = selectedBusId();
        devices = readDevices();
        table->setRowCount(devices.size());
        for (int row = 0; row < devices.size(); ++row) {
            const auto &device = devices.at(row);
            const QStringList columns = {device.busId, device.name, device.vendorProduct,
                device.driver, device.hub ? "Hub - not shareable" : device.status};
            for (int column = 0; column < columns.size(); ++column)
                table->setItem(row, column, new QTableWidgetItem(columns.at(column)));
            if (device.busId == selected)
                table->selectRow(row);
        }
        updateControls();
    }

    QString selectedBusId() const
    {
        const int row = table->currentRow();
        return row >= 0 && table->item(row, 0) ? table->item(row, 0)->text() : QString();
    }

    void updateControls()
    {
        const bool installed = QFileInfo::exists(USBIP_HELPER_PATH);
        const int row = table->currentRow();
        const bool selected = row >= 0 && row < devices.size();
        share->setEnabled(!busy && installed && selected && !devices.at(row).hub && !devices.at(row).shared);
        unshare->setEnabled(!busy && installed && selected && devices.at(row).shared);
        start->setEnabled(!busy && installed && !serverRunning);
        stop->setEnabled(!busy && installed && serverRunning);
        if (!installed)
            hint->setText("Administrative helper not installed. AppImage: run "
                          "sudo ./helper/install-helper.sh from the distribution folder. "
                          "Source build: sudo cmake --install build.");
        else if (selected && devices.at(row).shared)
            hint->setText("Client command: sudo usbip attach -r <this-server-IP> -b " + devices.at(row).busId);
        else
            hint->setText("Select a device to share. Changes require administrator authorization. "
                          "Device status refreshes every 3 seconds. Sharing is not persisted across reboot.");
    }

    void checkServer()
    {
        if (serverQuery.state() != QProcess::NotRunning)
            return;
        serverQuery.disconnect(this);
        connect(&serverQuery, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                serverStatus->setText("Cannot query systemd: " + serverQuery.errorString());
                log->append("Cannot query systemd: " + serverQuery.errorString());
            }
        });
        connect(&serverQuery, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                const QString state = QString::fromUtf8(serverQuery.readAllStandardOutput()).trimmed();
                const QString error = QString::fromUtf8(serverQuery.readAllStandardError()).trimmed();
                serverRunning = state == "active";
                serverStatus->setText("Server: " + (state.isEmpty() ? "unknown" : state)
                    + (error.isEmpty() ? QString() : " - " + error));
                updateControls();
            });
        serverQuery.start("/usr/bin/systemctl", {"is-active", "usbipd.service"});
    }

    void changeSharing(bool sharing)
    {
        const int row = table->currentRow();
        if (row < 0 || row >= devices.size())
            return;
        const UsbDevice device = devices.at(row);
        QString message = sharing
            ? "Sharing " + device.name + " (" + device.busId + ") disconnects it from local applications "
              "and makes it available to remote clients."
            : "Unsharing " + device.name + " (" + device.busId + ") disconnects any attached remote client.";
        if (sharing && device.inputDevice)
            message += "\n\nWARNING: This is an input/HID device. Sharing a keyboard or mouse "
                       "may leave you unable to control this computer.";
        message += "\n\nContinue?";
        if (QMessageBox::warning(this, sharing ? "Share device" : "Unshare device", message,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            operate({sharing ? "share" : "unshare", device.busId});
    }

    void operate(const QStringList &arguments)
    {
        busy = true;
        updateControls();
        statusBar()->showMessage("Waiting for authorization / completing operation...");
        auto *process = new QProcess(this);
        auto *timeout = new QTimer(process);
        timeout->setSingleShot(true);
        timeout->setInterval(120000);
        connect(timeout, &QTimer::timeout, this, [this] {
            log->append("Authorization/operation is taking longer than expected. "
                        "Check the authorization dialog and system service state.");
            statusBar()->showMessage("Still waiting for authorization / operation...");
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                busy = false;
                log->append("Cannot start authorization: " + process->errorString());
                QMessageBox::critical(this, "Operation failed", process->errorString());
                statusBar()->clearMessage();
                updateControls();
                process->deleteLater();
            }
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, arguments](int code, QProcess::ExitStatus status) {
                const QString output = QString::fromUtf8(process->readAllStandardOutput()
                    + process->readAllStandardError()).trimmed();
                const bool success = status == QProcess::NormalExit && code == 0;
                const QString result = arguments.join(' ') + (success ? ": completed" : ": failed");
                log->append(result + (output.isEmpty() ? QString() : "\n" + output));
                if (!success)
                    QMessageBox::critical(this, "Operation failed",
                        output.isEmpty() ? "Authorization was canceled or the operation failed "
                                           "(exit " + QString::number(code) + ")." : output);
                busy = false;
                statusBar()->showMessage(result, 5000);
                refreshDevices();
                checkServer();
                process->deleteLater();
            });
        process->start("/usr/bin/pkexec", QStringList{USBIP_HELPER_PATH} + arguments);
        timeout->start();
    }

    QVector<UsbDevice> devices;
    QTableWidget *table;
    QLabel *serverStatus;
    QLabel *hint;
    QPushButton *start;
    QPushButton *stop;
    QPushButton *share;
    QPushButton *unshare;
    QTextEdit *log;
    QTimer timer;
    QProcess serverQuery;
    bool busy = false;
    bool serverRunning = false;
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setApplicationName("USBIP Manager");
    application.setOrganizationName("USBIPManager");
    MainWindow window;
    window.show();
    if (application.arguments().contains("--smoke-test"))
        QTimer::singleShot(500, &application, &QCoreApplication::quit);
    return application.exec();
}
