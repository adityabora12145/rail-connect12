// Project: Rail Connect (RailConnect_Qt)
// A Qt Widgets C++ train reservation system using data structures (LinkedList, Queue, File storage)
// Files included in this single document. Split them into separate files as indicated by the markers.

// -----------------------------
// FILE: CMakeLists.txt
// -----------------------------

cmake_minimum_required(VERSION 3.16)
project(RailConnectQt VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)

add_executable(RailConnect
    main.cpp
    mainwindow.h
    mainwindow.cpp
    models.h
    models.cpp
)

target_link_libraries(RailConnect PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

// -----------------------------
// FILE: models.h
// -----------------------------

#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QQueue>
#include <QMap>

// Train structure
struct Train {
    QString trainId;
    QString name;
    QString source;
    QString destination;
    int totalSeats;
    int bookedSeats;
    double baseFare;

    QJsonObject toJson() const;
    static Train fromJson(const QJsonObject &obj);
};

// Passenger
struct Passenger {
    QString name;
    int age;
    QString gender;
    QString pnr;
    QString trainId;
    int seatNo;
    double fare;

    QJsonObject toJson() const;
    static Passenger fromJson(const QJsonObject &obj);
};

// BookingDatabase holds trains and bookings using data structures
class BookingDatabase {
public:
    BookingDatabase();

    // train operations
    void addTrain(const Train &t);
    QVector<Train> searchTrains(const QString &src, const QString &dst) const;
    Train* findTrain(const QString &trainId);

    // booking operations
    bool bookTicket(const QString &trainId, const Passenger &p);
    bool cancelTicket(const QString &pnr);
    Passenger* findPassenger(const QString &pnr);

    // persistence
    bool loadFromFiles();
    bool saveToFiles() const;

    QVector<Train> trains;
    QVector<Passenger> passengers; // simple vector for booked passengers
    QQueue<Passenger> waitingList; // queue for waiting passengers

private:
    QString trainsFile = "trains.json";
    QString bookingsFile = "bookings.json";
};

#endif // MODELS_H

// -----------------------------
// FILE: models.cpp
// -----------------------------

#include "models.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>

QJsonObject Train::toJson() const {
    QJsonObject obj;
    obj["trainId"] = trainId;
    obj["name"] = name;
    obj["source"] = source;
    obj["destination"] = destination;
    obj["totalSeats"] = totalSeats;
    obj["bookedSeats"] = bookedSeats;
    obj["baseFare"] = baseFare;
    return obj;
}

Train Train::fromJson(const QJsonObject &obj) {
    Train t;
    t.trainId = obj["trainId"].toString();
    t.name = obj["name"].toString();
    t.source = obj["source"].toString();
    t.destination = obj["destination"].toString();
    t.totalSeats = obj["totalSeats"].toInt();
    t.bookedSeats = obj["bookedSeats"].toInt();
    t.baseFare = obj["baseFare"].toDouble();
    return t;
}

QJsonObject Passenger::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["age"] = age;
    obj["gender"] = gender;
    obj["pnr"] = pnr;
    obj["trainId"] = trainId;
    obj["seatNo"] = seatNo;
    obj["fare"] = fare;
    return obj;
}

Passenger Passenger::fromJson(const QJsonObject &obj) {
    Passenger p;
    p.name = obj["name"].toString();
    p.age = obj["age"].toInt();
    p.gender = obj["gender"].toString();
    p.pnr = obj["pnr"].toString();
    p.trainId = obj["trainId"].toString();
    p.seatNo = obj["seatNo"].toInt();
    p.fare = obj["fare"].toDouble();
    return p;
}

BookingDatabase::BookingDatabase() {
    // attempt load on construction
    loadFromFiles();
}

void BookingDatabase::addTrain(const Train &t) {
    trains.append(t);
}

QVector<Train> BookingDatabase::searchTrains(const QString &src, const QString &dst) const {
    QVector<Train> res;
    for (const Train &t: trains) {
        if (t.source.compare(src, Qt::CaseInsensitive) == 0 &&
            t.destination.compare(dst, Qt::CaseInsensitive) == 0) {
            res.append(t);
        }
    }
    return res;
}

Train* BookingDatabase::findTrain(const QString &trainId) {
    for (int i = 0; i < trains.size(); ++i) {
        if (trains[i].trainId == trainId) return &trains[i];
    }
    return nullptr;
}

bool BookingDatabase::bookTicket(const QString &trainId, const Passenger &p) {
    Train *t = findTrain(trainId);
    if (!t) return false;
    if (t->bookedSeats < t->totalSeats) {
        // seat available
        Passenger np = p;
        int seat = ++(t->bookedSeats);
        np.seatNo = seat;
        // dynamic fare: simple: baseFare + 1% per booked seat
        np.fare = t->baseFare * (1.0 + 0.01 * t->bookedSeats);
        // generate PNR
        np.pnr = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
        passengers.append(np);
        saveToFiles();
        return true;
    } else {
        // put to waiting list
        waitingList.enqueue(p);
        saveToFiles();
        return true;
    }
}

bool BookingDatabase::cancelTicket(const QString &pnr) {
    for (int i = 0; i < passengers.size(); ++i) {
        if (passengers[i].pnr == pnr) {
            QString trainId = passengers[i].trainId;
            // free seat
            Train *t = findTrain(trainId);
            if (t) t->bookedSeats = qMax(0, t->bookedSeats - 1);
            passengers.removeAt(i);
            // allocate seat to first waiting passenger if any
            if (!waitingList.isEmpty()) {
                Passenger w = waitingList.dequeue();
                // book for same train if matches trainId, otherwise find another
                if (w.trainId.isEmpty()) w.trainId = trainId;
                bookTicket(w.trainId, w);
            }
            saveToFiles();
            return true;
        }
    }
    return false;
}

Passenger* BookingDatabase::findPassenger(const QString &pnr) {
    for (int i = 0; i < passengers.size(); ++i) {
        if (passengers[i].pnr == pnr) return &passengers[i];
    }
    return nullptr;
}

bool BookingDatabase::loadFromFiles() {
    // trains
    QFile f(trainsFile);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray data = f.readAll();
        QJsonDocument d = QJsonDocument::fromJson(data);
        if (d.isArray()) {
            QJsonArray arr = d.array();
            trains.clear();
            for (const QJsonValue &v: arr) {
                trains.append(Train::fromJson(v.toObject()));
            }
        }
        f.close();
    } else {
        // create sample trains if file missing
        trains.clear();
        Train t1{"123A","Express One","Mumbai","Pune",100,0,200.0};
        Train t2{"456B","Coastal Mail","Chennai","Bangalore",80,0,350.0};
        Train t3{"789C","InterCity","Delhi","Agra",120,0,150.0};
        trains.append(t1); trains.append(t2); trains.append(t3);
        saveToFiles();
    }

    // bookings
    QFile bf(bookingsFile);
    if (bf.open(QIODevice::ReadOnly)) {
        QByteArray data = bf.readAll();
        QJsonDocument d = QJsonDocument::fromJson(data);
        if (d.isObject()) {
            QJsonObject obj = d.object();
            passengers.clear();
            waitingList.clear();
            QJsonArray parr = obj["passengers"].toArray();
            for (const QJsonValue &v: parr) passengers.append(Passenger::fromJson(v.toObject()));
            QJsonArray warr = obj["waiting"].toArray();
            for (const QJsonValue &v: warr) waitingList.enqueue(Passenger::fromJson(v.toObject()));
        }
        bf.close();
    }
    return true;
}

bool BookingDatabase::saveToFiles() const {
    // trains
    QJsonArray tarr;
    for (const Train &t: trains) tarr.append(t.toJson());
    QJsonDocument td(tarr);
    QFile tf(trainsFile);
    if (tf.open(QIODevice::WriteOnly)) {
        tf.write(td.toJson());
        tf.close();
    }

    // bookings
    QJsonObject obj;
    QJsonArray parr;
    for (const Passenger &p: passengers) parr.append(p.toJson());
    obj["passengers"] = parr;
    QJsonArray warr;
    for (int i = 0; i < waitingList.size(); ++i) {
        warr.append(waitingList.at(i).toJson());
    }
    obj["waiting"] = warr;
    QJsonDocument bd(obj);
    QFile bf(bookingsFile);
    if (bf.open(QIODevice::WriteOnly)) {
        bf.write(bd.toJson());
        bf.close();
    }
    return true;
}

// -----------------------------
// FILE: mainwindow.h
// -----------------------------

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include "models.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSearch();
    void onBook();
    void onCancel();
    void onShowAll();

private:
    BookingDatabase db;

    // widgets
    QLineEdit *srcEdit;
    QLineEdit *dstEdit;
    QPushButton *searchBtn;
    QTableWidget *trainsTable;

    QLineEdit *nameEdit;
    QLineEdit *ageEdit;
    QLineEdit *genderEdit;
    QLineEdit *bookTrainIdEdit;
    QPushButton *bookBtn;

    QLineEdit *cancelPnrEdit;
    QPushButton *cancelBtn;

    QTextEdit *logView;

    void setupUi();
    void log(const QString &s);
};

#endif // MAINWINDOW_H

// -----------------------------
// FILE: mainwindow.cpp
// -----------------------------

#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("Rail Connect — Train Reservation System");
    resize(900, 600);

    QVBoxLayout *mainLay = new QVBoxLayout(central);

    // Search area
    QHBoxLayout *searchLay = new QHBoxLayout();
    srcEdit = new QLineEdit(); srcEdit->setPlaceholderText("Source");
    dstEdit = new QLineEdit(); dstEdit->setPlaceholderText("Destination");
    searchBtn = new QPushButton("Search Trains");
    QPushButton *showAllBtn = new QPushButton("Show All Trains");
    searchLay->addWidget(new QLabel("Search:"));
    searchLay->addWidget(srcEdit);
    searchLay->addWidget(dstEdit);
    searchLay->addWidget(searchBtn);
    searchLay->addWidget(showAllBtn);
    mainLay->addLayout(searchLay);

    trainsTable = new QTableWidget();
    trainsTable->setColumnCount(6);
    trainsTable->setHorizontalHeaderLabels({"Train ID","Name","Source","Destination","Seats (Booked/Total)","Base Fare"});
    trainsTable->horizontalHeader()->setStretchLastSection(true);
    mainLay->addWidget(trainsTable, 3);

    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(showAllBtn, &QPushButton::clicked, this, &MainWindow::onShowAll);

    // Booking form
    QGroupBox *bookBox = new QGroupBox("Book Ticket");
    QGridLayout *bgrid = new QGridLayout(bookBox);
    nameEdit = new QLineEdit(); nameEdit->setPlaceholderText("Passenger Name");
    ageEdit = new QLineEdit(); ageEdit->setPlaceholderText("Age");
    genderEdit = new QLineEdit(); genderEdit->setPlaceholderText("Gender");
    bookTrainIdEdit = new QLineEdit(); bookTrainIdEdit->setPlaceholderText("Train ID to book");
    bookBtn = new QPushButton("Book");
    bgrid->addWidget(new QLabel("Name:"),0,0); bgrid->addWidget(nameEdit,0,1);
    bgrid->addWidget(new QLabel("Age:"),1,0); bgrid->addWidget(ageEdit,1,1);
    bgrid->addWidget(new QLabel("Gender:"),2,0); bgrid->addWidget(genderEdit,2,1);
    bgrid->addWidget(new QLabel("Train ID:"),3,0); bgrid->addWidget(bookTrainIdEdit,3,1);
    bgrid->addWidget(bookBtn,4,0,1,2);
    mainLay->addWidget(bookBox);
    connect(bookBtn, &QPushButton::clicked, this, &MainWindow::onBook);

    // Cancellation form
    QGroupBox *cancelBox = new QGroupBox("Cancel Ticket");
    QHBoxLayout *cLay = new QHBoxLayout(cancelBox);
    cancelPnrEdit = new QLineEdit(); cancelPnrEdit->setPlaceholderText("PNR to cancel");
    cancelBtn = new QPushButton("Cancel");
    cLay->addWidget(cancelPnrEdit); cLay->addWidget(cancelBtn);
    mainLay->addWidget(cancelBox);
    connect(cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancel);

    // log view
    logView = new QTextEdit(); logView->setReadOnly(true);
    mainLay->addWidget(new QLabel("System Log:"));
    mainLay->addWidget(logView,1);

    // show initial trains
    onShowAll();
}

void MainWindow::log(const QString &s) {
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    logView->append(ts + " — " + s);
}

void MainWindow::onShowAll() {
    trainsTable->setRowCount(0);
    for (const Train &t: db.trains) {
        int r = trainsTable->rowCount();
        trainsTable->insertRow(r);
        trainsTable->setItem(r,0,new QTableWidgetItem(t.trainId));
        trainsTable->setItem(r,1,new QTableWidgetItem(t.name));
        trainsTable->setItem(r,2,new QTableWidgetItem(t.source));
        trainsTable->setItem(r,3,new QTableWidgetItem(t.destination));
        trainsTable->setItem(r,4,new QTableWidgetItem(QString("%1/%2").arg(t.bookedSeats).arg(t.totalSeats)));
        trainsTable->setItem(r,5,new QTableWidgetItem(QString::number(t.baseFare)));
    }
}

void MainWindow::onSearch() {
    QString s = srcEdit->text().trimmed();
    QString d = dstEdit->text().trimmed();
    if (s.isEmpty() || d.isEmpty()) {
        QMessageBox::warning(this, "Input needed", "Please enter both source and destination.");
        return;
    }
    QVector<Train> res = db.searchTrains(s,d);
    trainsTable->setRowCount(0);
    for (const Train &t: res) {
        int r = trainsTable->rowCount();
        trainsTable->insertRow(r);
        trainsTable->setItem(r,0,new QTableWidgetItem(t.trainId));
        trainsTable->setItem(r,1,new QTableWidgetItem(t.name));
        trainsTable->setItem(r,2,new QTableWidgetItem(t.source));
        trainsTable->setItem(r,3,new QTableWidgetItem(t.destination));
        trainsTable->setItem(r,4,new QTableWidgetItem(QString("%1/%2").arg(t.bookedSeats).arg(t.totalSeats)));
        trainsTable->setItem(r,5,new QTableWidgetItem(QString::number(t.baseFare)));
    }
    log(QString("Searched trains: %1 -> %2 (found %3)").arg(s).arg(d).arg(res.size()));
}

void MainWindow::onBook() {
    QString name = nameEdit->text().trimmed();
    int age = ageEdit->text().toInt();
    QString gender = genderEdit->text().trimmed();
    QString trainId = bookTrainIdEdit->text().trimmed();
    if (name.isEmpty() || age<=0 || gender.isEmpty() || trainId.isEmpty()) {
        QMessageBox::warning(this, "Missing info", "Please fill all passenger and train ID fields.");
        return;
    }
    Passenger p;
    p.name = name; p.age = age; p.gender = gender; p.trainId = trainId;
    bool ok = db.bookTicket(trainId, p);
    if (ok) {
        Passenger *np = nullptr;
        // find last passenger with same name (crudely)
        for (int i = db.passengers.size()-1; i>=0; --i) {
            if (db.passengers[i].name == p.name && db.passengers[i].trainId == p.trainId) { np = &db.passengers[i]; break; }
        }
        if (np) {
            QMessageBox::information(this, "Booked", QString("Ticket booked. PNR: %1\nSeat: %2\nFare: %3").arg(np->pnr).arg(np->seatNo).arg(np->fare));
            log(QString("Booked: %1 on %2 (PNR %3)").arg(np->name).arg(np->trainId).arg(np->pnr));
        } else {
            QMessageBox::information(this, "Waiting List", "Train full: passenger added to waiting list.");
            log(QString("Added to waiting list: %1 for %2").arg(p.name).arg(p.trainId));
        }
        onShowAll();
    } else {
        QMessageBox::warning(this, "Failed", "Booking failed (train not found).");
    }
}

void MainWindow::onCancel() {
    QString pnr = cancelPnrEdit->text().trimmed();
    if (pnr.isEmpty()) { QMessageBox::warning(this, "Missing", "Enter PNR to cancel."); return; }
    bool ok = db.cancelTicket(pnr);
    if (ok) {
        QMessageBox::information(this, "Cancelled", "Ticket cancelled successfully.");
        log(QString("Cancelled PNR: %1").arg(pnr));
        onShowAll();
    } else {
        QMessageBox::warning(this, "Not found", "PNR not found.");
    }
}

// -----------------------------
// FILE: main.cpp
// -----------------------------

#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

// -----------------------------
// End of document
// -----------------------------

// Notes:
// - Split the sections into separate files exactly as labeled: CMakeLists.txt, models.h/cpp, mainwindow.h/cpp, main.cpp
// - Requires Qt6 (Widgets). If you have Qt5, minor changes to CMake may be needed.
// - The project writes trains.json and bookings.json into the current working directory for persistence.
// - This implementation uses QVector (array-like), QQueue (waiting list), and simple dynamic pricing logic.
// - You can extend: add admin authentication, reports, PNR search UI, seat layout, file encryption, or switch to binary files.
