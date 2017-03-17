#include "opencv2/opencv.hpp"
#include <fstream>
#include <stdlib.h>
#include <signal.h>
#include <string>

int g_is_inv = 0;
int (*g_logic)(cv::Mat&);

/*
 * 7 8 9     ^
 * 4 5 6 = < N >
 * 1 2 3     v
 */
enum MOVE_DIRECTION {
  MOVE_BACK    = 2,
  MOVE_LEFT    = 4,
  MOVE_STOP    = 5,
  MOVE_RIGHT   = 6,
  MOVE_UPPER_R = 7,
  MOVE_FOWARD  = 8,
  MOVE_UPPER_L = 9,
};

struct th_main_data
{
  int is_running;        // thread exit flag
  cv::VideoCapture *cap; // video capture object
};
struct th_main_data g_data_main = {1, NULL};

const int T = 50 * 1000 * 1000; // 100 [msec]
struct th_pwm_data
{
  int is_running; // thread exit flag
  int period;     // pwm T[usec]
  int duty;       // pwm duty[%]
  int gpio;       // gpioXX
};

struct th_pwm_data g_data_r_f = {1, T, 0, 67};
struct th_pwm_data g_data_r_b = {1, T, 0, 69};
struct th_pwm_data g_data_l_f = {1, T, 0, 71};
struct th_pwm_data g_data_l_b = {1, T, 0, 73};


void* pwm_loop(void* data)
{
  struct th_pwm_data *d = (struct th_pwm_data *)data;
  char path[BUFSIZ];
  snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", d->gpio);
  std::ofstream out(path);
  out << "0";

  while(d->is_running)
    {
      struct timespec on_period  = {0, d->period * (d->duty / 100.0)};
      struct timespec off_period = {0, d->period * (1.0 - (d->duty / 100.0))};
      out << "1" << std::endl;  
      nanosleep(&on_period, NULL);
      out << "0" << std::endl;
      nanosleep(&off_period, NULL);
    }

  out << "0";
  return NULL;
}


void move_car (int direction)
{
  switch (direction)
    {
    case MOVE_BACK:
      g_data_l_f.duty = 0;
      g_data_l_b.duty = 100;
      g_data_r_f.duty = 0;
      g_data_r_b.duty = 100;
      break;
    case MOVE_LEFT:
      g_data_l_f.duty = 100;
      g_data_l_b.duty = 0;
      g_data_r_f.duty = 0;
      g_data_r_b.duty = 100;
      break;
    case MOVE_RIGHT:
      g_data_l_f.duty = 0;
      g_data_l_b.duty = 100;
      g_data_r_f.duty = 100;
      g_data_r_b.duty = 0;
      break;
    case MOVE_FOWARD:
      g_data_l_f.duty = 100;
      g_data_l_b.duty = 0;
      g_data_r_f.duty = 100;
      g_data_r_b.duty = 0;
      break;
    case MOVE_STOP:
      g_data_l_f.duty = 0;
      g_data_l_b.duty = 0;
      g_data_r_f.duty = 0;
      g_data_r_b.duty = 0;
    default:
      break;
    }


}

int raster_scan (cv::Mat &frame)
{
  cv::Mat gray_img;
  cv::Mat bin_img; 
  cv::cvtColor(frame, gray_img, CV_BGR2GRAY);
  cv::threshold(gray_img,bin_img,0,255,
               (g_is_inv ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY) | cv::THRESH_OTSU);
  //cv::threshold(gray_img,bin_img,50,255,
  //              (g_is_inv ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY));
  cv::GaussianBlur(bin_img, bin_img, cv::Size(11,11), 10, 10);

  int cnt = 0, cnt_max = 0, current_px = 0, start_px = 0, flag = 0;
  for (int i = 0; i < bin_img.size().width; i++)
    {
      char px = bin_img.at<unsigned char>(bin_img.size().height/4, i);
      if ((px == 0) && (flag == 0))
        {
          flag = 1;
          cnt = 1;
          current_px = i;
        }
      if ((px == 0) && (flag == 1))
        {
          cnt++;
        }
      if ((px == 255) || i == bin_img.size().width - 1)
        {
          flag = 0;
          if (cnt_max < cnt)
            {
              cnt_max = cnt;
              start_px = current_px;
            }
          cnt = 0;
        }
    }
  int center_px = start_px + cnt_max / 2;


  frame = bin_img;
  cv::circle(frame, cv::Point(center_px ,bin_img.size().height/2), 20, cv::Scalar(0,0,255), 3, 4);

  if (center_px > 0 && center_px < bin_img.size().width * 3 / 8)
    {
      return MOVE_LEFT;
    }
  else if (center_px > bin_img.size().width * 5 / 8)
    {
      return MOVE_RIGHT;
    }
  else
    {
      return MOVE_FOWARD;
    }
}

void *main_loop (void *data)
{
  struct th_main_data *d = (struct th_main_data *)data;

  while(d->is_running)
    {
      cv::Mat frame;
      (*(d->cap)) >> frame; // get a new frame from camera

      move_car(g_logic(frame));

      cv::imwrite("/var/tmp/img.jpg", frame);
    }

  return NULL;
}

int main(int argc, char* argv[])
{
  g_logic = raster_scan;

  if ((argc == 2) && (argv[1][0] = '1'))
    {
      g_is_inv = 1;
    }

  cv::VideoCapture cap(1); // /dev/video*

  if(!cap.isOpened())
    {
      return -1;
    }

  g_data_main.cap = &cap;
    
  pthread_t th_main, th_pwm_r_f,th_pwm_r_b, th_pwm_l_f,th_pwm_l_b;
  pthread_create(&th_main, NULL, main_loop, &g_data_main);
  pthread_create(&th_pwm_r_f, NULL, pwm_loop, &g_data_r_f);
  pthread_create(&th_pwm_r_b, NULL, pwm_loop, &g_data_r_b);
  pthread_create(&th_pwm_l_f, NULL, pwm_loop, &g_data_l_f);
  pthread_create(&th_pwm_l_b, NULL, pwm_loop, &g_data_l_b);

  int sig;
  sigset_t block_mask;
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGINT);
  sigaddset(&block_mask, SIGTERM);
  sigprocmask(SIG_SETMASK, &block_mask, NULL);

  while (1)
    {
      if (sigwait(&block_mask, &sig) == 0)
        {
          if (sig == SIGINT || sig == SIGTERM) break;
          else continue;
        }
    }

  g_data_main.is_running = 0;
  pthread_join(th_main, NULL);

  g_data_r_f.is_running = 0;
  pthread_join(th_pwm_r_f, NULL);

  g_data_r_b.is_running = 0;
  pthread_join(th_pwm_r_b, NULL);

  g_data_l_f.is_running = 0;
  pthread_join(th_pwm_l_f, NULL);

  g_data_l_b.is_running = 0;
  pthread_join(th_pwm_l_b, NULL);
  

  return 0;
}
