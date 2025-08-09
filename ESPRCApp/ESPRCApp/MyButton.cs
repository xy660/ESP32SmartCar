using System;
using System.Collections.Generic;
using System.Text;

namespace ESPRC遥控器
{
    internal class MyButton : Xamarin.Forms.Button
    {
        public event EventHandler onPressed;
        public virtual void OnPressed()
        {

        }
    }
}
