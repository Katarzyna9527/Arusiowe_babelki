//samochod
#include <sys/types.h>
#include "ros/ros.h"
#include "std_msgs/String.h"
#include <cstdlib>
#include "map/car_init.h"//finish the address
#include "crossings/autocross_msg.h"
#include "vizualization/auto_viz.h"
#include <ctime>

  enum states{
     crossing,
     crossed,
     askingForDir,
     sendingDir,
     drivingTowCrossing,
     waitingForDir,
     waiting,
     tailgate
   };

class Car{
  
private:
   bool isCrossed;		//informuje o tym, czy samochód opuścił skrzyżowanie
   int16_t speed;
   int16_t newLenght;
   int16_t carID;		//samochód  zgłaszający albo do którego kierowana jest odpowiedź
   int16_t direction;		//przy pierwszym zapytaniu = -1
   int16_t lenght;		//odległość do naspnego skrzyżowania
   int16_t nextCrossID;		//
   int16_t previousCrossID;	//
   int16_t previousAutoID;
   crossings::autocross_msg msgA;//crossings
   vizualization::auto_viz msgV;
   states state;
   ros::Publisher carViz;
   std::vector<int16_t> avaibleDirections;	//dosprtępne skręty

public:
   
   //ros::NodeHandle viz;
   ros::NodeHandle n;
   //bool gotMessage;

   void choseADir(){
     while(true){
      int a = std::rand()%4;
      if(avaibleDirections[a]>0){
           direction = a;
	   break;}
      }
   }

   void carCallback(const crossings::autocross_msg::ConstPtr& msg){//crossings::
      if(!msg->isMsgFromAuto) {
          //gotMessage=true;
	  ROS_INFO("I got msg from crossing with ID:%d", msg->nextCrossID);
	  readTheMessage(msg);
      }
    }

   Car(){
		
      ros::ServiceClient client = n.serviceClient<map::car_init>("init_car");//map
      //map::
      map::car_init srv;
      uint8_t ReqID = 0;
      srv.request.req = (uint8_t) ReqID;
      if (client.call(srv)){
         carID=srv.response.carID;
         lenght = srv.response.pathLenght;
         previousCrossID =srv.response.prevCrossing;
         nextCrossID = srv.response.nextCrossing;
         isCrossed=false;
         speed = 1;
         direction = -1;
         state=askingForDir;
         //gotMessage=false;
         carViz = n.advertise<vizualization::auto_viz>("auto_viz", 1000);
         fillMessageViz();
         carViz.publish(msgV);
         ROS_INFO("iniciated, asking for directions");
      }
      else{
         ROS_ERROR("Failed to call service car_init");
      }
   }

   //crossings::
   crossings::autocross_msg fillTheMessage(){
       msgA.isMsgFromAuto = true;
       msgA.autoID = carID;        
       msgA.direction = direction;     
       msgA.isCrossed = isCrossed;      
       msgA.length = lenght;       
       msgA.nextCrossID = nextCrossID;
       return msgA;
   }
   
   void fillMessageViz(){
      msgV.autoID=carID;
      msgV.startCrossID=previousCrossID;
      msgV.endCrossID=nextCrossID;
      msgV.distance=lenght;
   }

   void readTheMessage(const crossings::autocross_msg::ConstPtr& msg){//crossings::
        if(!msg->isMsgFromAuto){    
         if(state == waiting){
           newLenght = msg->length;       
           nextCrossID = msg->nextCrossID;
	   speed=1;
           state = crossing;
           ROS_INFO("crossing");
         }
         else if(state == waitingForDir){
           
           previousAutoID = msg->previousAutoID;
           avaibleDirections = msg->availableDirections;
           if(previousAutoID==0){ROS_INFO("reading");
             choseADir();
             state = drivingTowCrossing;
             ROS_INFO("driving towards crossing");
           }
           else{
             state = tailgate;
             ROS_INFO("tailgating");
           }
         }
       }
   }
   
   void moveFrd(){ROS_INFO("%d", lenght);
       lenght = lenght - speed;
       if(lenght<=5 && state == drivingTowCrossing){
         speed = 0;
         state = sendingDir;
         ROS_INFO("sending the direction I wanna go to the crossing");
       }
       else if(lenght == 0){
         lenght=newLenght;
       }
       else if(lenght == newLenght-5 && state == crossing){
         isCrossed=true;
         state = crossed;
         ROS_INFO("crossed");
       }
       fillMessageViz();
       carViz.publish(msgV);
   }

   void changeState(states newState){
     state = newState;
     if(newState == askingForDir){
        isCrossed=false;
     }
   }

   int16_t giveNextCrossing(){
     return nextCrossID;
   }

   states giveState(){
     return state;
   }

   void setSpeed(int newSpeed){
     speed = newSpeed;
   }

};

int main(int argc, char **argv)
{
  ROS_INFO("iniciation in progress");
  states state;
  int16_t instance = 0;
  //crossings::
  crossings::autocross_msg msgB;
  time_t tm=std::time(NULL);
  time_t tmsg=std::time(NULL);
  std::srand( std::time( NULL ) );
  ros::init(argc, argv, "car_node", ros::init_options::AnonymousName);
  
  Car car;
  std::stringstream ss;
  ss << "crossing_" << car.giveNextCrossing();
  
  ros::Publisher carPub = car.n.advertise<crossings::autocross_msg>(ss.str().c_str(), 1000);//crossing
  ros::Subscriber carSub = car.n.subscribe(ss.str().c_str(), 1000, &Car::carCallback, &car);
  ros::Rate loop_rate(10);

   while(carPub.getNumSubscribers()<2)
   {
      ROS_ERROR("Waiting for crossing to cathch on");
      sleep(1);
   }

  while(ros::ok()){
    if(std::time(NULL)>tm+1){
       car.moveFrd();
       tm=std::time(NULL);
    }
    state = car.giveState();
    if(state == askingForDir || state == sendingDir || state == crossed){
       msgB = car.fillTheMessage();
       carPub.publish(msgB);
       //car.gotMessage=false;
       if(state == askingForDir){
         car.changeState(waitingForDir);
         ROS_INFO("waiting for possible directions");
       }
       else if(state == crossed){
         msgB = car.fillTheMessage();
         //carPub.publish(msgB);
         ss.str("");
         ss.clear();
         ss << "crossing_" << car.giveNextCrossing();
         carPub = car.n.advertise<crossings::autocross_msg>(ss.str().c_str(), 1000);//crossing
         carSub = car.n.subscribe(ss.str().c_str(), 1000, &Car::carCallback, &car);
         car.changeState(askingForDir);
         ROS_INFO("asking for possible directions");
       }
       else if(state == sendingDir){
         car.changeState(waiting);
         ROS_INFO("waiting for a permission to enter the crossing");
       }
    }
    //else if((state == waiting || state == waitingForDir)&& !car.gotMessage){
          //carPub.publish(msgB);ROS_INFO("5");
          //tmsg=std::time(NULL);
    ///}
    ros::spinOnce();
  }
}
