// Mario Combat reference-capture driver (injected as root frame 1 DoAction of a COPY of the SWF).
// Overrides Key.isDown with a scripted key table, jumps into a level, and streams the hero's
// per-tick state to a local Python server (tools/refcap/refcap_server.py) over XMLSocket.
// Screenshots: on a "shot" step the driver sends SHOT, and on the next tick busy-waits so the
// window keeps showing the shot tick's render while the server grabs the screen.
stop();
Stage.showMenu = false;
_global.__keys = {};
_global.__plan = [];
_global.__planIdx = 0;
_global.__tick = -1;
_global.__running = false;
_global.__hold = 0;
_global.__level = 486;
_global.__settle = 40;
_global.__zoom = 0;
_global.__isDown = function(c)
{
   return _global.__keys[c] == true;
};
// Key and its methods are write-protected built-ins: clear the protection flags, then override.
ASSetPropFlags(Key,null,0,7);
ASSetPropFlags(_global,"Key",0,7);
Key.isDown = _global.__isDown;
if(Key.isDown != _global.__isDown)
{
   _global.Key = {isDown:_global.__isDown,getCode:function()
   {
      return 0;
   },addListener:function()
   {
   },removeListener:function()
   {
   }};
}
var sock = new XMLSocket();
_global.__sock = sock;
sock.onConnect = function(ok)
{
   if(ok)
   {
      _global.__sock.send("HELLO|keyOverride=" + (Key.isDown == _global.__isDown));
   }
};
sock.onData = function(s)
{
   var parts = s.split("|");
   if(parts[0] == "PLAN")
   {
      // PLAN|rootFrame|settleTicks|zoom|t,keys,shot;t,keys,shot;...   keys = '+'-separated key codes
      _global.__level = Number(parts[1]);
      _global.__settle = Number(parts[2]);
      _global.__zoom = Number(parts[3]);
      var steps = parts[4].split(";");
      var i = 0;
      while(i < steps.length)
      {
         var f = steps[i].split(",");
         _global.__plan.push({t:Number(f[0]),keys:f[1],shot:f[2]});
         i++;
      }
      _root.gotoAndPlay(_global.__level);
      _global.__running = true;
   }
};
_global.__applyKeys = function(spec)
{
   _global.__keys = {};
   if(spec != "" && spec != undefined && spec != "END")
   {
      var ks = spec.split("+");
      var j = 0;
      while(j < ks.length)
      {
         _global.__keys[Number(ks[j])] = true;
         j++;
      }
   }
};
_global.__state = function(h)
{
   var c = h.getInstanceAtDepth(-16381);
   var a = h.attack;
   var u = h.hurt_mc;
   var k = "";
   for(var key in _global.__keys)
   {
      k += key + "+";
   }
   var r = [_global.__tick,h._x,h._y,h.xSpeed,h.grav,h._currentframe,h._xscale,h.onGround,h.onLeftWall,h.onRightWall,h.walling,h.fighting,h.hurt,h.skiding,h.shortHop,h.stopSliding,h.jumpAnimThres,h.pressingAttack,h.life,c._name,c._currentframe,a._currentframe,u._currentframe,_root.vcam._xscale,k];
   return r.join(",");
};
_root.createEmptyMovieClip("__drv",1048000);
_root.__drv.onEnterFrame = function()
{
   if(_global.__hold > 0)
   {
      var t0 = getTimer();
      while(getTimer() - t0 < _global.__hold)
      {
      }
      _global.__hold = 0;
   }
   if(!_global.__running)
   {
      return undefined;
   }
   var h = _root.hero;
   if(h == undefined || h.activated != true)
   {
      return undefined;
   }
   if(_global.__zoom > 0)
   {
      _root.vcam._xscale = _global.__zoom;
      _root.vcam._yscale = _global.__zoom;
   }
   _global.__tick++;
   _global.__sock.send("S|" + __state(h));
   var t = _global.__tick - _global.__settle;
   var step = _global.__plan[_global.__planIdx];
   while(step != undefined && t >= step.t)
   {
      if(step.shot != "" && step.shot != undefined)
      {
         _global.__sock.send("SHOT|" + step.shot + "|" + _global.__tick);
         _global.__hold = 400;
      }
      if(step.keys == "END")
      {
         _global.__running = false;
         _global.__sock.send("END|" + _global.__tick);
         return undefined;
      }
      if(step.keys != "=")
      {
         __applyKeys(step.keys);
      }
      _global.__planIdx++;
      step = _global.__plan[_global.__planIdx];
   }
};
sock.connect("127.0.0.1",8642);
