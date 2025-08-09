using System.Net;
using System.Net.Sockets;
using System.Text;
namespace ESPRCServer
{
    internal class Program
    {
        static string errorPage = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nCao Ni Ma , Diao Ni Ma Bi Wo Jie Kou Ne";
        static int port = 1234;
        class ConnectPair
        {
            public Socket? Client = null;
            public Socket? ESP = null;
            public Socket? ClientCamView = null;
            public Socket? ESPCamera = null;
        }
        static object lockObject = new object();
        static string logPath = Environment.CurrentDirectory + "/ESPRCServer.log";
        static string authPath = Environment.CurrentDirectory + "/ESPRCAuth.txt";
        static void Log(string msg)
        {
            string message = $"[LOG][{DateTime.Now}] {msg}";
            Console.WriteLine(message);
            lock (lockObject)
            {
                var fs = new FileStream(logPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read);
                fs.Position = fs.Length;
                fs.Write(Encoding.UTF8.GetBytes(message + "\r\n"));
                fs.Close();
            }
        }
        static Dictionary<long,ConnectPair> kvClient = new Dictionary<long,ConnectPair>();
        static List<long> AuthedKeys = new List<long>();
        static byte[] recvCorrectly(Socket s,int len)
        {
            byte[] buf = new byte[len];
            int recved = 0;
            while (recved < len)
            {
                int tmp = s.Receive(buf, recved, len - recved, SocketFlags.None);
                if (tmp == 0)
                {
                    throw new Exception("socket has closed");
                }
                recved += tmp;
            }
            return buf;
        }
        static unsafe void sendCommand(Socket s,int c1,int c2 = 0,int c3 = 0,int c4 = 0)
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
        static void handClientRC(Socket s, long key)
        {
            Log("Client connected key=" + key);
            try
            {
                while (true)
                {
                    var data = recvCorrectly(s, 16);          
                    long flag = BitConverter.ToInt32(data, 0);
                    Socket? active_ESP_car = kvClient[key].ESP;
                    if (active_ESP_car != null)
                    {
                        try
                        {
                            active_ESP_car.Send(data, 16, SocketFlags.None);
                        }
                        catch
                        {
                            if (flag != 2)
                            {
                                sendCommand(s, 0, 1);
                            }
                        }
                    }
                    else
                    {
                        if (flag != 2)
                        {
                            sendCommand(s, 0, 1);
                        }
                    }
                }
            }
            catch
            {

            }
        }
        static void handClientESP(Socket s,long key)
        {
            Log("ESP connected key=" + key);
            try
            {
                while (true)
                {
                    var data = recvCorrectly(s, 16);
                    Socket? active_RC = kvClient[key].Client;
                    if (active_RC != null)
                    {
                        active_RC.Send(data, 16, SocketFlags.None);
                    }
                }
            }
            catch 
            { 
            }
        }
        static void handClientCamView(Socket s,long key)
        {
            Log("Client Camera View connected key=" + key);
            try
            {
                while (true)
                {
                    var data = recvCorrectly(s, 16);
                    Socket? espCam = kvClient[key].ESPCamera;
                    if(espCam != null)
                    {
                        espCam.Send(data,16,SocketFlags.None);
                    }
                }
            }
            catch
            {
            }
        }
        static void handESPCamera(Socket s,long key)
        {
            Log("ESP Camera connected key=" + key);
            try
            {
                while (true)
                {
                    byte[] lenbuf = recvCorrectly(s, 4);
                    int len = BitConverter.ToInt32(lenbuf, 0);
                    byte[] data = recvCorrectly(s, len);
                    Socket? clientView = kvClient[key].ClientCamView;
                    if(clientView != null)
                    {
                        clientView.Send(lenbuf,4,SocketFlags.None);
                        clientView.Send(data,len,SocketFlags.None);
                    }
                }
            }
            catch
            {
            }
        }
        static void handClient(Socket s)
        {
            var data = recvCorrectly(s, 16);
            int flag = BitConverter.ToInt32(data, 0);
            long key = BitConverter.ToInt64(data, 4);
            if(!AuthedKeys.Contains(key))
            {
                s.Close();
                Log("not auth key:" + key + "  flag:" + flag);
                return;
            }
            if (flag == 10) //ESP
            {
                if(!kvClient.ContainsKey(key))
                {
                    kvClient.Add(key, new ConnectPair());
                }
                kvClient[key].ESP = s;
                handClientESP(s,key);
            }
            else if (flag == 11) //Client
            {
                if (!kvClient.ContainsKey(key))
                {
                    kvClient.Add(key, new ConnectPair());
                }
                kvClient[key].Client = s;
                handClientRC(s,key);
            }
            else if(flag == 20) //ESP Camera FPV
            {
                if (!kvClient.ContainsKey(key))
                {
                    kvClient.Add(key, new ConnectPair());
                }
                kvClient[key].ESPCamera = s;
                handESPCamera(s, key);
            }
            else if(flag == 21) //Client FPV View
            {
                if (!kvClient.ContainsKey(key))
                {
                    kvClient.Add(key, new ConnectPair());
                }
                kvClient[key].ClientCamView = s;
                handClientCamView(s, key);
            }
            else
            {
                s.Close();
                Log("bad connection");
            }
        }
        static void ListenClient(Socket s)
        {
            Log($"listen addr {((IPEndPoint)s.LocalEndPoint).Address.ToString()}");
            while(true)
            {
                var ns = s.Accept();
                Task.Run(() =>
                {
                    handClient(ns);                   
                });
            }
        }
        static void Main(string[] args)
        {
            Log($"listen port:{port}");
            //AuthedKeys.Add(11451419);
            string[] authlist = File.ReadAllText(authPath).Split(',');
            foreach(var cc in authlist)
            {
                AuthedKeys.Add(long.Parse(cc));
                Log("add auth id " + cc);
            }

            foreach(var addr in Dns.GetHostAddresses(Dns.GetHostName()))
            {
                if(addr.AddressFamily == AddressFamily.InterNetwork)
                {
                    var s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                    s.Bind(new IPEndPoint(addr, port));
                    s.Listen(20);
                    Task.Run(() =>
                    {
                        ListenClient(s);
                    });                  
                }
            }
            var local = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            local.Bind(new IPEndPoint(IPAddress.Parse("127.0.0.1"), port));
            local.Listen(20);
            ListenClient(local);
        }
    }
}
