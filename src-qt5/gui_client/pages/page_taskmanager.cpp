//===========================================
//  PC-BSD source code
//  Copyright (c) 2016, PC-BSD Software/iXsystems
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "page_taskmanager.h"
#include "ui_page_taskmanager.h" //auto-generated from the .ui file

#define memStyle QString("QLabel{background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:%1p rgb(100,100,200), stop:%2pa rgb(200,100,100), stop:%2pb rgb(200,100,100), stop:%3pa rgb(100,200,100), stop:%3pb rgb(100,200,100), stop:%4pa rgb(230, 230, 230), stop:%4pb rgb(230, 230, 230), stop:%5p white);\nborder: 1px solid black;\nborder-radius: 3px;}")

taskmanager_page::taskmanager_page(QWidget *parent, sysadm_client *core) : PageWidget(parent, core), ui(new Ui::taskmanager_ui){
  ui->setupUi(this);	
  connect(ui->push_kill, SIGNAL(clicked()), this, SLOT(slot_kill_proc()) );
}

taskmanager_page::~taskmanager_page(){
  
}

//Initialize the CORE <-->Page connections
void taskmanager_page::setupCore(){
  connect(CORE, SIGNAL(newReply(QString, QString, QString, QJsonValue)), this, SLOT(ParseReply(QString, QString, QString, QJsonValue)) );
}

//Page embedded, go ahead and startup any core requests
void taskmanager_page::startPage(){
  //Let the main window know the name of this page
  emit ChangePageTitle( tr("Task Manager") );

  //Now run any CORE communications
  slotRequestProcInfo();
  slotRequestMemInfo();
  slotRequestCPUInfo();
	
  // Get info every 3 seconds
  proctimer = new QTimer(this);
  connect(proctimer, SIGNAL(timeout()), this, SLOT(slotRequestProcInfo()));
  connect(proctimer, SIGNAL(timeout()), this, SLOT(slotRequestMemInfo()));
  connect(proctimer, SIGNAL(timeout()), this, SLOT(slotRequestCPUInfo()));
  proctimer->start(3000);

  qDebug() << "Start page!";
}


// === PRIVATE SLOTS ===
void taskmanager_page::ParseReply(QString id, QString namesp, QString name, QJsonValue args){
  if( !id.startsWith("page_task_man_")){ return; }
 // qDebug() << "reply" << id;

  // Read in the PID list
  if ( id == "page_task_man_taskquery")
  {
    if ( ! args.isObject() )
      return;
    parsePIDS(args.toObject());
  }
  else if(id=="page_task_man_mem_check"){
    if(name!="error" && namesp!="error"){
      if(args.isObject() && args.toObject().contains("memorystats")){
        QJsonObject memO = args.toObject().value("memorystats").toObject();
	//qDebug() << "Got memory info:" << args << memO;
	int active, cache, freeM, inactive, wired; //MB
	active = cache = freeM = inactive = wired = 0;
	if(memO.contains("active")){ active = memO.value("active").toString().toInt(); }
	if(memO.contains("cache")){ cache = memO.value("cache").toString().toInt(); }
	if(memO.contains("free")){ freeM = memO.value("free").toString().toInt(); }
	if(memO.contains("inactive")){ inactive = memO.value("inactive").toString().toInt(); }
	if(memO.contains("wired")){ wired = memO.value("wired").toString().toInt(); }
	ShowMemInfo(active, cache, freeM, inactive, wired);
      }
    }
    
  }else if(id=="page_task_man_cpu_check" && name!="error" && namesp!="error" ){
    //CPU usage
    qDebug() << "Got CPU Usage:" << args;
	  
    slotRequestCPUTempInfo(); //always make sure the general CPU info comes back first
  }else if(id=="page_task_man_cputemp_check" && name!="error" && namesp!="error" ){
    //CPU Temperature
    qDebug() << "Got CPU Temps:" << args;
  }else{
    //Reply to a process action - go ahead and update the list now
    slotRequestProcInfo();
  }
}

void taskmanager_page::parsePIDS(QJsonObject jsobj)
{
  //ui->taskWidget->clear();

  //qDebug() << "KEYS" << jsobj.keys();
  QStringList keys = jsobj.keys();
  if (keys.contains("message") ) {
    qDebug() << "MESSAGE" << jsobj.value("message").toString();
    return;
  }

  // Look for procinfo
  QJsonObject procobj = jsobj.value("procinfo").toObject();
  QStringList pids = procobj.keys();
  for ( int i=0; i < pids.size(); i++ )
  {
    QString PID = pids.at(i);
    QJsonObject pidinfo = procobj.value(PID).toObject();

    // Check if we have this PID already
    QList<QTreeWidgetItem *> foundItems = ui->taskWidget->findItems(PID, Qt::MatchExactly, 0);
    if ( ! foundItems.isEmpty() )
    {
      foundItems.at(0)->setText(1, pidinfo.value("username").toString());
      foundItems.at(0)->setText(2, pidinfo.value("thr").toString());
      foundItems.at(0)->setText(3, pidinfo.value("pri").toString());
      foundItems.at(0)->setText(4, pidinfo.value("nice").toString());
      foundItems.at(0)->setText(5, pidinfo.value("size").toString());
      foundItems.at(0)->setText(6, pidinfo.value("res").toString());
      foundItems.at(0)->setText(7, pidinfo.value("state").toString());
      foundItems.at(0)->setText(8, pidinfo.value("cpu").toString());
      foundItems.at(0)->setText(9, pidinfo.value("time").toString());
      foundItems.at(0)->setText(10, pidinfo.value("wcpu").toString());
      foundItems.at(0)->setText(11, pidinfo.value("command").toString());
    } else {
      // Create the new taskWidget item
      new QTreeWidgetItem(ui->taskWidget, QStringList() << PID
          << pidinfo.value("username").toString()
          << pidinfo.value("thr").toString()
          << pidinfo.value("pri").toString()
          << pidinfo.value("nice").toString()
          << pidinfo.value("size").toString()
          << pidinfo.value("res").toString()
          << pidinfo.value("state").toString()
          << pidinfo.value("cpu").toString()
          << pidinfo.value("time").toString()
          << pidinfo.value("wcpu").toString()
          << pidinfo.value("command").toString()
          );
    }
  }

  // Now loop through the UI and look for pids to remove
  for ( int i=0; i<ui->taskWidget->topLevelItemCount(); i++ ) {
    // Check if this item exists in the PID list
    if ( ! pids.contains(ui->taskWidget->topLevelItem(i)->text(0)) ) {
      // No? Remove it
      ui->taskWidget->takeTopLevelItem(i);
    }
  }

  // Resize the widget to the new contents
  ui->taskWidget->header()->resizeSections(QHeaderView::ResizeToContents);
  ui->taskWidget->sortItems(10, Qt::DescendingOrder);
}

void taskmanager_page::ShowMemInfo(int active, int cache, int freeM, int inactive, int wired){
  //assemble the stylesheet
  int total = active+cache+freeM+inactive+wired;
  QString style = memStyle;
  QString TT = tr("Memory Stats (MB)")+"\n";
    TT.append( QString(tr(" - Total: %1")).arg(QString::number(total))+"\n");
  // Wired memory
  double perc = qRound( 10000.0* (wired/( (double) total)) )/10000.0;
  double laststop = 0;
    TT.append( QString(tr(" - Wired: %1 (%2)")).arg(QString::number(wired), QString::number(perc*100.0)+"%") +"\n");
    style.replace("%1p",QString::number(perc)); //placement
    style.replace("%2pa",QString::number(perc+0.00001)); //start of next section
    laststop = perc+0.00001;
  double totperc = perc;
  //Active memory
  perc = qRound( 10000.0* (active/( (double) total)) )/10000.0;
  totperc+=perc;
    TT.append( QString(tr(" - Active: %1 (%2)")).arg(QString::number(active), QString::number(perc*100.0)+"%") +"\n");
    style.replace("%2pb",QString::number( (totperc>laststop) ? totperc : (laststop+0.000001) ));
    style.replace("%3pa",QString::number(totperc+0.00001)); //start of next section
    laststop = totperc+0.00001;
  //cache memory
  perc = qRound( 10000.0* (cache/( (double) total)) )/10000.0;
  totperc+=perc;
    TT.append( QString(tr(" - Cache: %1 (%2)")).arg(QString::number(cache), QString::number(perc*100.0)+"%")+"\n" );
    style.replace("%3pb",QString::number( (totperc>laststop) ? totperc : (laststop+0.000001) ));
    style.replace("%4pa",QString::number(totperc+0.0001) ); //start of next section 
    laststop = totperc+0.0001;
  ui->label_mem_stats->setText( QString(tr("Total Memory: %1 MB,   Used: %2%")).arg(QString::number(total), QString::number(totperc*100.0)) );
  //inactive memory
  perc = qRound( 10000.0* (inactive/( (double) total)) )/10000.0;
  totperc+=perc;
    TT.append( QString(tr(" - Inactive: %1 (%2)")).arg(QString::number(inactive), QString::number(perc*100.0)+"%") +"\n");
    style.replace("%4pb",QString::number( (totperc>laststop) ? totperc : (laststop+0.000001) ));
    style.replace("%5p",QString::number(totperc+0.00001)); //start of next section
  //free memory
    TT.append( QString(tr(" - Free: %1 (%2)")).arg(QString::number(freeM), QString::number((1-totperc)*100.0)+"%") );
  //Now set the widgets
  //qDebug() << "styleSheet:" << style;
  ui->label_mem_stats->setToolTip(TT);
  ui->label_mem_stats->setStyleSheet(style);
}

void taskmanager_page::slotRequestProcInfo(){
  QJsonObject jsobj;
  jsobj.insert("action", "procinfo");
  CORE->communicate("page_task_man_taskquery", "sysadm", "systemmanager", jsobj);
}

void taskmanager_page::slotRequestMemInfo(){
  QJsonObject jsobj;
  jsobj.insert("action", "memorystats");
  CORE->communicate("page_task_man_mem_check", "sysadm", "systemmanager", jsobj);
}

void taskmanager_page::slotRequestCPUInfo(){
  QJsonObject jsobj;
  jsobj.insert("action", "cpupercentage");
  CORE->communicate("page_task_man_cpu_check", "sysadm", "systemmanager", jsobj);	
}

void taskmanager_page::slotRequestCPUTempInfo(){
  QJsonObject jsobj;
  jsobj.insert("action", "cputemps");
  CORE->communicate("page_task_man_cputemp_check", "sysadm", "systemmanager", jsobj);	
}

void taskmanager_page::slot_kill_proc(){
  //get the currently-selected PID
  QTreeWidgetItem *tmp = ui->taskWidget->currentItem();
  if(tmp==0){ return; }
  QString pid = tmp->text(0); //column 0 is the PID
  //Now send the request
  QJsonObject jsobj;
  jsobj.insert("action", "killproc");
  jsobj.insert("pid",pid);
  jsobj.insert("signal","KILL");
  CORE->communicate("page_task_man_kill_proc", "sysadm", "systemmanager", jsobj);	  
}