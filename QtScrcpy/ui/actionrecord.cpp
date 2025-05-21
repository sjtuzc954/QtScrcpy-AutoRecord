#include "actionrecord.h"
#include <QDebug>
#include "../QtScrcpyCore/include/QtScrcpyCore.h"
#include "QDir"
#include <QClipboard>
#include <QShortcut>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QThread>

ActionRecord::ActionRecord(QWidget *parent) : QWidget(parent), ui(new Ui::ActionRecord) {
    ui->setupUi(this);
    ui->comboBox->addItem("OnePlus");
    ui->comboBox->addItem("Huawei");
    ui->comboBox->setCurrentIndex(0);

    ui->episodeSpin->setMaximum(1000);
    ui->episodeSpin->setMinimum(1);
    ui->stepSpin->setMaximum(1000);
    ui->stepSpin->setMinimum(1);
    ui->episodeSpin->setValue(1);
    ui->stepSpin->setValue(1);

    ui->atomicComboBox->addItem("PickDate");
    ui->atomicComboBox->setCurrentIndex(0);

    this->isRecording = false;

    // auto shortcut = new QShortcut(QKeySequence("Ctrl+d"), this);
    // shortcut->setAutoRepeat(false);
    // connect(shortcut, &QShortcut::activated, this, [this]() {
    //     this->step();
    // });

    connect(ui->startAtomicButton, &QPushButton::clicked, this, &ActionRecord::startAtomic);
    connect(ui->endAtomicButton, &QPushButton::clicked, this, &ActionRecord::endAtomic);
}

ActionRecord::~ActionRecord()
{
    delete ui;
}

// ActionRecord &ActionRecord::getInstance()
// {
//     static ActionRecord instance;
//     return instance;
// }

void ActionRecord::appendAction(const QString &action)
{
    this->curStepActions.append(action);
}

void ActionRecord::on_startButton_clicked()
{
    ui->recordingLabel->setText("正在录制...");
    this->curEpsActions.clear();
    this->curStepActions.clear();
    this->isRecording = true;
    // auto device = qsc::IDeviceManage::getInstance().getDevice(this->serial);
    // if (!device) {
    //     return;
    // }
    // QString filename = QString("%1/%2/%3.jpg")
    //                        .arg(ui->comboBox->currentText())
    //                        .arg(ui->episodeSpin->text())
    //                        .arg(ui->stepSpin->text());
    // device->screenshotWithFilename(filename);

    // const QString& recordRootPath = device->getDeviceParams().recordPath;
    // QDir dir(recordRootPath);
    // QString absolutePath = dir.absoluteFilePath(filename.replace(".jpg", ".xml"));
    // dumpXml(absolutePath);
}

void ActionRecord::on_endButton_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(this->serial);
    if (!device) {
        return;
    }

    QString filename = QString("%1/%2/%3.jpg")
                           .arg(ui->comboBox->currentText())
                           .arg(ui->episodeSpin->text())
                           .arg(ui->stepSpin->text());

    device->screenshotWithFilename(filename);

    const QString& recordRootPath = device->getDeviceParams().recordPath;


    QDir curEpsRootDir(recordRootPath);
    curEpsRootDir.cd(QString("%1/%2/").arg(ui->comboBox->currentText(), ui->episodeSpin->text()));
    QStringList filters;
    filters << "*.jpg";
    QFileInfoList fileInfoList = curEpsRootDir.entryInfoList(filters, QDir::Files);
    qInfo() << fileInfoList.size();

    for (const auto &fileInfo : qAsConst(fileInfoList)) {
        QString baseName = fileInfo.baseName();
        bool ok = false;
        int num = baseName.toInt(&ok);
        if (ok && num > ui->stepSpin->value()) {
            if (QFile::remove(fileInfo.absoluteFilePath())) {
                qInfo() << "Deleted:" << fileInfo.absoluteFilePath();
            } else {
                qInfo() << "Failed to delete:" << fileInfo.absoluteFilePath();
            }
        }
    }

    QString absolutePath = curEpsRootDir.absoluteFilePath("actions.log");
    QFile logFile(absolutePath);
    if (!logFile.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate)) {
        qInfo() << "Open file " << filename << " failed.";
        return;
    }
    QString content = QString("task: %1\n").arg(ui->taskEdit->text());
    for (auto& s : this->curEpsActions) {
        content += s + '\n';
    }
    logFile.write(content.toStdString().c_str());
    logFile.close();

    ui->stepSpin->setValue(1);
    ui->episodeSpin->stepBy(1);
    qInfo() << "action log saved to " << absolutePath;

    ui->recordingLabel->setText("录制结束");
    this->curEpsActions.clear();
    this->curStepActions.clear();
    this->isRecording = false;
}

void ActionRecord::setSerial(const QString &serial)
{
    this->serial = serial;
}

bool ActionRecord::recording()
{
    return this->isRecording;
}

void ActionRecord::screenshot()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(this->serial);
    if (!device) {
        return;
    }

    QString filename = QString("%1/%2/%3.jpg")
                           .arg(ui->comboBox->currentText())
                           .arg(ui->episodeSpin->text())
                           .arg(ui->stepSpin->text());

    device->screenshotWithFilename(filename);
}

void ActionRecord::step()
{
    if (!recording())
        return;
    this->screenshot();
    this->stepWithoutScreenshot();
}

void ActionRecord::stepWithoutScreenshot()
{
    QString trans = QString("%1 --> %2:").arg(QString::number(ui->stepSpin->value())).arg(QString::number(ui->stepSpin->value() + 1));
    qInfo() << trans;
    this->curEpsActions.append(trans);
    for (auto& action : this->curStepActions) {
        qInfo() << action;
        this->curEpsActions.append(action);
    }
    this->curStepActions.clear();

    ui->stepSpin->stepBy(1);
}

void ActionRecord::loadTasks()
{
    this->tasks.clear();
    this->tasks.insert("Lifestyle", {{"Food-Delivery", {}}, {"Shopping", {}}, {"Traveling", {}}});
    this->tasks.insert("System", {{"Settings", {}}, {"Interaction", {}}});
    this->tasks.insert("Tools", {{"Browser", {}}, {"Productivity", {}}});
    this->tasks.insert("Multimedia", {{"Music", {}}, {"Video", {}}});
    this->tasks.insert("Communication", {{"Community", {}}, {"Email", {}}, {"Social-Networking", {}}, {"Instant-Messaging", {}}});

    auto device = qsc::IDeviceManage::getInstance().getDevice(this->serial);
    if (!device) {
        return;
    }

    const QString& recordRootPath = device->getDeviceParams().recordPath;
    QString filename = QString("tasks.json");
    QDir dir(recordRootPath);
    QString absolutePath = dir.absoluteFilePath(filename);
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadWrite)) {
        qInfo() << "Open file " << filename << " failed.";
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &jsonError);
    file.close();

    if (jsonError.error != QJsonParseError::NoError && !doc.isNull()) {
        qInfo() << "Parse json file " << filename << " failed.";
        return;
    }

    QJsonObject rootObj = doc.object();
    for (auto it = this->tasks.begin(); it != this->tasks.end(); ++it) {
        QJsonValue subdomainValue = rootObj.value(it.key());
        if (!subdomainValue.isObject()) {
            qInfo() << "Parse json file " << filename << " failed at key \"" << it.key() << "\".";
            return;
        }
        QJsonObject subdomainObject =subdomainValue.toObject();
        auto& subdomainMap = it.value();
        for (auto it2 = subdomainMap.begin(); it2 != subdomainMap.end(); ++it2) {
            QJsonValue taskArrayValue = subdomainObject.value(it2.key());
            if (!taskArrayValue.isArray()) {
                qInfo() << "Parse json file " << filename << " failed at key \"" << it2.key() << "\".";
                return;
            }
            QJsonArray taskArray = taskArrayValue.toArray();
            for (int i = 0; i < taskArray.size(); ++i) {
                QJsonValue taskValue = taskArray.at(i);
                if (!taskValue.isString()) {
                    qInfo() << "Parse json file " << filename << " failed at key \"" << it2.key() << "\".";
                    return;
                }
                it2.value().append(taskValue.toString());
            }
        }
    }
}

bool ActionRecord::fakeModeActivated()
{
    return false;
}

void ActionRecord::bufferedPress(int x, int y)
{
    if (!recording())
        return;
    this->lock.lock();
    this->bufferedPressTime = QDateTime::currentMSecsSinceEpoch();
    this->bufferedPressPos = qMakePair(x, y);
    this->screenshot();
    this->lock.unlock();
}

void ActionRecord::bufferedRelease(int x, int y)
{
    if (!recording())
        return;
    this->lock.lock();
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    if (this->bufferedPressTime < 0 || ts < this->bufferedPressTime) {
        this->lock.unlock();
        return;
    }
    qint64 diff = ts - this->bufferedPressTime;
    int xDist = x - bufferedPressPos.first, yDist = y - bufferedPressPos.second;
    int dist = xDist * xDist + yDist * yDist;
    if (diff >= 700 && dist <= 100) {
        this->appendAction(QString("LONGCLICK [%1, %2]").arg(x).arg(y));
    } else {
        this->appendAction(QString("PRESS [%1, %2]").arg(bufferedPressPos.first).arg(bufferedPressPos.second));
        this->appendAction(QString("RELEASE [%1, %2]").arg(x).arg(y));
    }
    this->bufferedPressTime = -1;
    this->stepWithoutScreenshot();
    this->lock.unlock();
}

void ActionRecord::setAdbProcess(qsc::AdbProcess *adb)
{
    this->adb = adb;
}

void ActionRecord::on_lineEdit_returnPressed()
{
    if (!recording())
        return;
    auto device = qsc::IDeviceManage::getInstance().getDevice(this->serial);
    if (!device) {
        return;
    }
    this->lock.lock();
    QString input = ui->lineEdit->text();
    this->appendAction(QString("INPUT %1").arg(input));
    this->step();
    QClipboard *board = QApplication::clipboard();
    board->setText(input);
    emit device->setDeviceClipboard();
    board->clear();
    // device->postTextInput(input);
    ui->lineEdit->clear();
    this->lock.unlock();
}

void ActionRecord::startAtomic()
{
    if (!this->recording())
        return;
    this->lock.lock();
    ui->atomicLabel->setText(QString("当前原子能力：%1").arg(ui->atomicComboBox->currentText()));
    this->appendAction(QString("START ATOMIC %1").arg(ui->atomicComboBox->currentText()));
    this->lock.unlock();
}

void ActionRecord::endAtomic()
{
    if (!this->recording())
        return;
    this->lock.lock();
    ui->atomicLabel->setText(QString("当前原子能力：无"));
    this->curEpsActions.append(QString("END ATOMIC %1").arg(ui->atomicComboBox->currentText()));
    this->lock.unlock();
}


void ActionRecord::dumpXml(const QString &absPath)
{
    qDebug() << "try dump xml to " << absPath;
    QStringList adbArgs;
    adbArgs << "shell" << "uiautomator" << "dump";
    this->adb->execute(this->serial, adbArgs);
    if (!this->adb->waitForFinished())
        qDebug() << "dump xml timeout";
    adbArgs.clear();
    adbArgs << "pull" << "/sdcard/window_dump.xml" << absPath;
    this->adb->execute(this->serial, adbArgs);
    if (!this->adb->waitForFinished())
        qDebug() << "pull xml timeout";
    qDebug() << "xml dumped to " << absPath;
}

