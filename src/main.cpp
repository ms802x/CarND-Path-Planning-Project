#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

int lane = 1;
double ref_vel = 0;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];

          json msgJson;

          vector<double> next_x_vals;
          vector<double> next_y_vals;

          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */


     // my code block 1
         
// first we need to check if we had any previous point, otherwise the car will be stock. 
           if(previous_path_x.size() > 0) 
           car_s = end_path_s; 
           
          // this step is the prediction, here the data will be provided to the behavior planner 
            // then the behavior planner based on that will issue its suggestion
            bool slow_down = false, left_car=false,right_car=false; 
          //loop through the detected cars
          for(int i = 0; i < sensor_fusion.size(); i++) {
            int car_lane;
            float d = sensor_fusion[i][6]; 
          
            //Specifiy the car lane
            // each lane is 4 m wide so we 
            if(d > 0 && d < 4) 
              car_lane = 0;
            else if(d > 4 && d < 8) 
              car_lane = 1;
            else if(d > 8 && d < 12) 
              car_lane = 2;
             else 
              continue ;
  
            // calculate speed
            double vx = sensor_fusion[i][3];
            double vy = sensor_fusion[i][4];   
          // the magnitude of speed vector is 
            double speed =pow(pow(vx,2)+pow(vy,2),0.5);
            double s_coor =  sensor_fusion[i][5];
            
           // S coordinate predicition 
            s_coor +=((double)speed * 0.02 * previous_path_x.size());

            //set flags to true or false based on the vehicle location .
               if ( car_lane == lane ) 
             // Here the car lane = our lane
             // check if the target car is behind our car
              slow_down |= s_coor > car_s && s_coor - car_s < 30;
             else if ( car_lane - lane == -1 ) 
              // check car on the left lane
              left_car |= s_coor - 30 < s_coor && car_s + 30 > s_coor;
            else if ( car_lane - lane == 1 ) 
              //check car on right lane
              right_car |= car_s - 30 < s_coor && car_s + 30 > s_coor;
                   
           }
          
          
           
          // Behavioral Planning 
          double acc = 0.224; 
          double max_acc = 49.5;
           // Here only three finite state will be used
          //Keep Lane, change lane left,right
          
          // The behavior planner starts suggestion based on the given predtion
          if(slow_down) {
            if(!left_car && lane > 0) 
              // here we check that our car is not on left lane boundry+no car on the left. 
              //thus it can go left if car ahead
              lane--;
            
            else if(!right_car && lane < 2) 
              // here we check that our car is not on right lane boundry+no car on the right. 
              //thus it can go right if car ahead
              lane++;
            else 
              // just slow down 
              ref_vel -= acc; 
            
              } else {
            if(lane != 1) 
              if((lane == 0 && !right_car) || (lane == 2 && !left_car)) 
                 lane = 1; //back to the center lane
              
            
             if(ref_vel <  max_acc) 
              ref_vel +=acc;
            
          }      
       
          
          // trajectory generatation.
           vector<double> ptsx,ptsy;
           double ref_x = car_x, ref_y = car_y, ref_raw = deg2rad(car_yaw),ref_x_1,ref_y_1;
          
          // we need to make sure that we have data history (not 1 point only)
          if(previous_path_x.size()<2){
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);
            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);

            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          
          else{
            
          // creat a list of xn_-1 and xn
            ref_x = previous_path_x[previous_path_x.size() - 1];
            ref_y = previous_path_y[previous_path_x.size() - 1];
            ref_x_1 =  previous_path_x[previous_path_x.size() - 2];
            ref_y_1 =  previous_path_y[previous_path_x.size()- 2];
            ref_raw = atan2(ref_y - ref_y_1, ref_x - ref_x_1);

            ptsx.push_back(ref_x_1);
            ptsx.push_back(ref_x);
            ptsy.push_back(ref_y_1);
            ptsy.push_back(ref_y);
          }
                                                            
          
                                                         
vector<double> next_wp0 = getXY(car_s + 30, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
vector<double> next_wp1 = getXY(car_s + 60, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
vector<double> next_wp2 = getXY(car_s + 90, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
                                                            
          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);

          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);
                   
          
         // Shift the points
          // without this block of code the car starts out of the street 
   
          for(int i = 0; i < ptsx.size(); ++i) {
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;
            ptsx[i] = shift_x * cos(0 - ref_raw) - shift_y * sin(0 - ref_raw);
            ptsy[i] = shift_x * sin(0 - ref_raw) + shift_y * cos(0 - ref_raw);
          }

           // here we do the line fitting                                  
          // create a spline
          tk::spline s;
          // set (x, y) points to the spline
          s.set_points(ptsx, ptsy);
  
          for(int i = 0; i < previous_path_x.size(); i++) {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }

          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt(target_x * target_x + target_y * target_y);

          double x_add_on = 0;

          for(int i = 0; i <= 50 -previous_path_x.size(); ++i) {
            double N = target_dist / (0.02 * ref_vel/2.24);
            double x_point = x_add_on + target_x / N;
            double y_point = s(x_point);

            x_add_on = x_point;

            double x_ref = x_point;
            double y_ref = y_point;

            x_point = x_ref * cos(ref_raw) - y_ref * sin(ref_raw);
            y_point = x_ref * sin(ref_raw) + y_ref * cos(ref_raw);

            x_point += ref_x;
            y_point += ref_y;

            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }

          //block 1 end here
                                                            
                                                            
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}