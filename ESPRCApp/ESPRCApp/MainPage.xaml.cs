using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using Xamarin.Forms;

namespace ESPRC遥控器
{
    public partial class MainPage : ContentPage
    {
        public MainPage()
        {
            InitializeComponent();
            CodeVersionDisplay.Text = "版本：1.0";
        }

        private void ConnectBtn_Clicked(object sender, EventArgs e)
        {
            try
            {
                Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                s.Connect(new IPEndPoint(IPAddress.Parse(IPAddressInput.Text), int.Parse(PortInput.Text)));
                if(true) //登录条件
                {
                    var t = new RCPage(s,long.Parse(UserTokenInput.Text));
                    Application.Current.MainPage = new NavigationPage(t);
                }
            }
            catch(Exception ex)
            {
                DisplayAlert("无法连接至服务器", ex.Message, "确定");
            }
        }
    }
}
