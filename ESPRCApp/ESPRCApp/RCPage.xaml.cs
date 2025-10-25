using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.Net.Sockets;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;
using System.Threading;

namespace ESPRC遥控器
{
    [XamlCompilation(XamlCompilationOptions.Compile)]
    public partial class RCPage : ContentPage
    {
        Socket connection;
        long carRepliedTime = 0;
        float carVBat = 0.0f;
        bool pageClosed = false;
        public RCPage(Socket conn, long auth)
        {
            InitializeComponent();
            connection = conn;
            sendLogin(conn, auth);
            Task.Run(checkTimer);
            Task.Run(watchDog);
            Task.Run(recv);
            Task.Run(sendSilderValue);
            CarType.SelectedIndex = 0;
        }
        double prevRotation = 0;
        double prevPWMSpeed = 0;
        double prevWeaponDegree = 0;
        int prevRotateMotorState = 5; //3=left 4=right 5=cent
            
        void sendSilderValue()
        {
            while(!pageClosed)
            {
                this.Dispatcher.BeginInvokeOnMainThread(() =>
                {
                    if (CarType.SelectedIndex == 0) //差速车&舵机车发送转向比例值
                    {
                        if (prevRotation != FangXiangPan.Value)
                        {
                            prevRotation = FangXiangPan.Value;
                            sendCommand(connection, 1, 6, (int)FangXiangPan.Value);
                        }
                    }
                    else if (CarType.SelectedIndex == 1) //直流电机阿克曼车发送转向类型
                    {
                        int rotateState = 5;
                        if(FangXiangPan.Value <= 35)
                        {
                            rotateState = 3;
                        }
                        else if(FangXiangPan.Value >= 65)
                        {
                            rotateState = 4;
                        }
                        
                        if(prevRotateMotorState != rotateState)
                        {
                            prevRotateMotorState = rotateState;
                            sendCommand(connection, 1, rotateState);
                        }
                    }

                    if(prevPWMSpeed != CarPWMSpeed.Value) //如果速度变化发送速度
                    {
                        prevPWMSpeed = CarPWMSpeed.Value;
                        int engineSpeed = (int)CarPWMSpeed.Value;
                        int rotatePWM = 500;
                        sendCommand(connection, 3, engineSpeed, rotatePWM);
                    }

                    if(prevWeaponDegree != weaponDegree.Value)
                    {
                        prevWeaponDegree = weaponDegree.Value;
                        sendCommand(connection, 6, 1, (int)prevWeaponDegree);
                    }
                });
                Thread.Sleep(100);
            }
        }
        void checkTimer()
        {
            while(!pageClosed)
            {
                try
                {
                    sendCheck(connection);
                }               
                catch { }
                Thread.Sleep(1000);
            }
        }
        static byte[] recvCorrectly(Socket s, int len)
        {
            byte[] buf = new byte[len];
            int recved = 0;
            while (recved < len)
            {
                recved += s.Receive(buf, recved, len - recved, SocketFlags.None);
            }
            return buf;
        }
        void watchDog()
        {
            while (!pageClosed)
            {
                if (carRepliedTime < Environment.TickCount)
                {
                    this.Dispatcher.BeginInvokeOnMainThread(() =>
                    {
                        //MessageBox.Show($"收到小车回复，电压：{VBat:0.00}V");
                        CarInfoDisplay.TextColor = Color.Red;
                        CarInfoDisplay.Text = $"连接超时";
                    });
                }
            }
        }
        unsafe void recv()
        {
            try
            {
                while (true)
                {
                    var bt = recvCorrectly(connection, 16);
                    fixed (byte* b = bt)
                    {
                        int* p = (int*)b;
                        if (p[0] == 0)
                        {
                            if (p[1] == 1)
                            {
                                this.Dispatcher.BeginInvokeOnMainThread(() =>
                                {
                                    DisplayAlert("错误","小车未连接到服务器","确定");
                                });
                            }
                        }
                        else if (p[0] == 2)
                        {
                            float* pfl = (float*)b;
                            float VBat = pfl[1];
                            carRepliedTime = Environment.TickCount + 3000;
                            carVBat = VBat;
                            this.Dispatcher.BeginInvokeOnMainThread(() =>
                            {
                                //MessageBox.Show($"收到小车回复，电压：{VBat:0.00}V");
                                CarInfoDisplay.TextColor = Color.Black;
                                CarInfoDisplay.Text = $"小车电压：{VBat}V";
                            });
                        }
                        else if (p[0] == 3)
                        {
                            if (p[1] == 1)
                            {
                                this.Dispatcher.BeginInvokeOnMainThread(() =>
                                {
                                    DisplayAlert("", "电压计校准完成！", "确定");
                                });
                            }else if (p[1] == 2)
                            {
                                this.Dispatcher.BeginInvokeOnMainThread(() =>
                                {
                                    DisplayAlert("", "设置关机电压成功", "确定");
                                });
                            }
                            else if (p[1] == 3)
                            {
                                this.Dispatcher.BeginInvokeOnMainThread(() =>
                                {
                                    DisplayAlert("", "小车即将在5秒后休眠/关机", "确定");
                                });
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                this.Dispatcher.BeginInvokeOnMainThread(() =>
                {
                    DisplayAlert("连接断开", "原因：\r\n1.网络不稳定\r\n2.服务器身份验证失败", "确定");
                    pageClosed = true;
                    Application.Current.MainPage = new MainPage();
                });
            }
        }
        static unsafe void sendCommand(Socket s, int c1, int c2 = 0, int c3 = 0, int c4 = 0)
        {
            var sd = new byte[16];
            fixed (byte* p = sd)
            {
                int* psd = (int*)p;
                psd[0] = c1;
                psd[1] = c2;
                psd[2] = c3;
                psd[3] = c4;
            }
            s.Send(sd);
        }
        static unsafe void sendAdcOffset(Socket s, float vbat)
        {
            var sd = new byte[16];
            fixed (byte* p = sd)
            {
                int* psd = (int*)p;
                float* pf = (float*)p;
                psd[0] = 4;
                pf[1] = vbat;
                psd[2] = 0;
                psd[3] = 0;
            }
            s.Send(sd);
        }
        static unsafe void sendShutdownVol(Socket s, float vbat)
        {
            var sd = new byte[16];
            fixed (byte* p = sd)
            {
                int* psd = (int*)p;
                float* pf = (float*)p;
                psd[0] = 7;
                psd[1] = 1;
                pf[2] = vbat;
                psd[3] = 0;
            }
            s.Send(sd);
        }
        unsafe void sendControlCommand(Socket s,int cmd)
        {
            sendCommand(s, 1, cmd);
        }
        unsafe void sendCheck(Socket s)
        {
            sendCommand(s, 2);
        }
        unsafe void sendLogin(Socket s,long authid)
        {
            var sd = new byte[16];
            fixed (byte* p = sd)
            {
                int* psd = (int*)p;
                long* pf = (long*)&psd[1];
                psd[0] = 11;
                pf[0] = authid;
            }
            s.Send(sd);
        }

        private void ForwardBtn_Pressed(object sender, EventArgs e)
        {
            sendControlCommand(connection, 1);
        }

        private void ForwardBtn_Released(object sender, EventArgs e)
        {
            sendControlCommand(connection, 0);          
        }

        private void BackBtn_Pressed(object sender, EventArgs e)
        {
            sendControlCommand(connection, 2);
        }

        private void BackBtn_Released(object sender, EventArgs e)
        {
            sendControlCommand(connection, 0);
        }
        int lastChangeDirection = 0;
        private void FangXiangPan_ValueChanged(object sender, ValueChangedEventArgs e)
        {
            if(Environment.TickCount - lastChangeDirection > 100) //设置cd防止网络堵车和舵机烧坏
            {
                
                lastChangeDirection = Environment.TickCount;
            }
        }
        int lastChangeSpeed = 0;
        private void CarPWMSpeed_ValueChanged(object sender, ValueChangedEventArgs e)
        {
            if(Environment.TickCount - lastChangeSpeed > 300)
            {
                int engineSpeed = (int)CarPWMSpeed.Value;
                int rotatePWM = 900;
                sendCommand(connection, 3, engineSpeed, rotatePWM);
                lastChangeSpeed = Environment.TickCount;
            }
        }

        private void CommitVolBtn_Clicked(object sender, EventArgs e)
        {
            sendAdcOffset(connection, float.Parse(ActuallyVolt.Text));
        }
        private void shootButton_Pressed(object sender, EventArgs e)
        {
            sendCommand(connection, 6, 10, 2);
        }
        private void shootButton_Released(object sender, EventArgs e)
        {
            sendCommand(connection, 6, 10, 3);
        }

        private void SwitchToFPV_Clicked(object sender, EventArgs e)
        {
            FPVViewBody.IsVisible = true;
            MenuBody.IsVisible = false;
        }

        private void backButton_Clicked(object sender, EventArgs e)
        {
            FPVViewBody.IsVisible = false;
            MenuBody.IsVisible = true;
        }

        private void CommitShutdownVolBtn_Clicked(object sender, EventArgs e)
        {
            sendShutdownVol(connection,float.Parse(ActuallyVolt.Text));
        }

        private void StartSleepBtn_Clicked(object sender, EventArgs e)
        {
            if(SleepTimeEntry.Text == null || SleepTimeEntry.Text.Length == 0)
            {
                DisplayAlert("错误", "请输入要休眠的秒数", "确定");
                return;
            }
            sendCommand(connection,7,2,int.Parse(SleepTimeEntry.Text));
        }
    }
}