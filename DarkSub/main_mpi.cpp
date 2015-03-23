
#include "signalmessage.h"


//!TJM
#ifdef USE_MPI

#include "mpicontrolgui.h"
#include "ui_mpicontrolgui.h"

#include "mpiengine.h"
#include "mpiUser.h"
#include "mpigatherUser.h"

#include "mpimesgrecvr.h"
#include "mpiscatterUser.h"
#include "pipeReader.h"
#include "signalmessage.h"

#include "pipewriter.h"
#include "mpicalcrunner.h"
#include "mpi.h"
// mpic++ -m64 /usr/lib64/libtiff.so -L/home/mpi/madden/mpi_lld -Wl,-rpath,/home/epicsioc/QTStuff/qt4.8/lib -o CINController main.o mainwindow.o controlportthread.o streaminport.o imageprocessor.o debugwindow.o waveformgui.o biasclocksgui.o networking.o descramble_block.o tiff_maker.o remote.o fccd_caller.o moc_mainwindow.o moc_controlportthread.o moc_imageprocessor.o moc_waveformgui.o moc_biasclocksgui.o moc_streaminport.o    -L/home/epicsioc/QTStuff/qt4.8/lib -lQtGui -L/home/epicsioc/QTStuff/qt4.8/lib -L/usr/X11R6/lib64 -lQtNetwork -lQtCore -lpthread -lsufeng

#endif
int mpiBackMain(int argc, char *argv[]);

int mainqt(int argc, char *argv[]);
//extern "C" int dtm(int argc, char *argv[]);
int mpiFrontMain(int argc, char *argv[]);

int numprocs;


#ifdef USE_MPI

mpiUser *myMPI;


//
// points to image queues
//
// free queueis list of free image memory. data queue is images w/ data in them
// we have input and ouitput. input is generated by system data input tap, from detector
// detectir queues into input data queue, using ram from input free queye
// output is the finished mpi-processed images.
// mpi gather takes free images from otuptu free queue, and puts data
// into output data queue. file writer, output gui dequeus from data queue
// for save and sisplauy.


#endif

extern "C"  //!!TJM end
{
int main(int argc, char *argv[])
{
    char debugstr[256];

 qRegisterMetaType<guiSignalMessage>();
 qRegisterMetaType<imageSignalMessage>();
 qRegisterMetaType<mpiSignalMessage>();



 printf("Running MPI Code\n");

 printf("Process ID = %d\n",getpid());
fflush(stdout);


 //usleep(20000000);




    myMPI =new mpiUser();
  myMPI->setupMPI(argc,argv);


  printf("=====RANK is %d =========\n",myMPI->rank);

  fflush(stdout);
  //!! or debugging- so we can get debugger conn before anything happens
  char dbg[256];
 //!! gets(dbg);



  printf("Start debugger and hit regurn\n");
  fflush(stdout);

//  gets(debugstr);


    if (myMPI->rank==0)
    {
    // run program that receives data from camera, and sends to MPI nodes

        mpiFrontMain( argc, argv);

    }
    else if (myMPI->rank==1)
    {
        //run gui that recveives images from MPI after compuation.
         mpiBackMain(argc,argv);
    }
   else
    {

        //mpiMesgRecvr recv_thread(0,myMPI);
        //recv_thread.start();
        myMPI->mpiCalcLoop();


    }

    myMPI->shutdownMPI();

  //  MPI_Finalize();



}
}


#ifdef USE_MPI

int mpiBackMain(int argc, char *argv[])
{

    //gather writes fresh mpi calc'ed images, pipeWriter reads this queue
    imageQueue output_data_queue;

    // gather reads free images from thsi wueue.
    // mpiControlGui window sends old iamges to free queue.
    imageQueue output_free_queue;
    //pipe Writer writes images to this queue.
    // mpiControlGui reads this queue
    imageQueue output_display_queue;


    output_free_queue.fillQueue(100,1024*1024);

    QApplication a(argc, argv);
    mpiControlGui window(output_display_queue,output_free_queue);

    mpiGatherUser gather(myMPI,output_data_queue,output_free_queue);

    pipeWriter image_output(
                output_data_queue,
                output_display_queue);

    QThread pipe_thread;

    QThread gather_thread;

    //
    // Connect signals/slots
    //

    //conn gui buttons to gather obhect
    gather.connect(&window,
                   SIGNAL(guiState(guiSignalMessage)),
                   SLOT(getGuiSpecs(guiSignalMessage)),
                   Qt::QueuedConnection);




    // connect gathered output images to image output, file writer , pipe writer etc.
    image_output.connect(&gather,
                         SIGNAL(signaldataready_mpi(mpiSignalMessage)),
                         SLOT(newImage(mpiSignalMessage)),
                         Qt::QueuedConnection);


    window.connect(&image_output,
                   SIGNAL(finishedImage(mpiSignalMessage)),
                   SLOT(newImage(mpiSignalMessage)),
                   Qt::QueuedConnection);


    // connect gui buttons to image output, image writer
    image_output.connect(
                &window,
                SIGNAL(guiState(guiSignalMessage)),
                SLOT(gotGuiSetting(guiSignalMessage)),
                Qt::QueuedConnection);




    //
    // Connect gui to mpi. this allows gui messages to be transferred to all mpi
    // processes for setup, control, and start/stop. gui controls whole set of
    // processes this way. signal from gui, gets translated into an mpi message,
    // then in other processes, gets translated back into a signal.
    //




  myMPI->connect(
              &window,
              SIGNAL(guiState(guiSignalMessage)),
              SLOT(sendMPIGuiSettings(guiSignalMessage)),
              Qt::QueuedConnection);




  gather.moveToThread(&gather_thread);
  image_output.moveToThread(&pipe_thread);


   window.show();
    gather_thread.start();
    pipe_thread.start();
    a.exec();//RANK 1
    gather_thread.quit();
    gather_thread.wait();
    pipe_thread.quit();
    pipe_thread.wait();

    return 0;
}




//rcv data from cin over udp and send as mpi messages
int mpiFrontMain(int argc, char *argv[])
{
    QApplication a(argc, argv);


    imageQueue input_data_queue;
    imageQueue input_free_queue;



    input_free_queue.fillQueue(100,1024*1024);

    mpiScatterUser scatter(myMPI,input_free_queue,input_data_queue);

    pipeReader pipe_in(input_free_queue,input_data_queue);
    QThread pipe_thread;

    mpiMesgRecvr recv_thread(0,myMPI);


    //mwindow_glbl=&w;
   //!! stream_inglbl=&stream_in;


    // open Pipe will run on pipe thread, as it will generate images for ever...
    pipe_in.connect(&scatter,
                    SIGNAL(connectInput(guiSignalMessage)),
                    SLOT(openPipe(guiSignalMessage)),
                    Qt::QueuedConnection);


    // direct conn so we can kill inout. need nice to close pipe to prevent segv..
    pipe_in.connect(&scatter,
                    SIGNAL(disconnectInput()),
                    SLOT(closePipe()),
                    Qt::DirectConnection);



   scatter.connect(&pipe_in,
                   SIGNAL(newImageReady(imageSignalMessage)),
                   SLOT(newImage(imageSignalMessage)),
                   Qt::QueuedConnection);



     scatter.connect(
                myMPI,
               SIGNAL(signalGuiSettings(guiSignalMessage)),
               SLOT(gotMPIGuiSettings(guiSignalMessage)),
               Qt::QueuedConnection);








    pipe_in.moveToThread(&pipe_thread);
    //stream_in_thread.connect(&w, SIGNAL(sendTime(QString)), SLOT(setText(QString)));


    pipe_thread.start(QThread::HighestPriority);

    recv_thread.start();


    a.exec(); //rank0                 // infinite loop,  everything from here on is driven by events (mouse clicks, signals, etc).
    pipe_thread.quit();
    pipe_thread.wait();
    return 0;
}
#endif
