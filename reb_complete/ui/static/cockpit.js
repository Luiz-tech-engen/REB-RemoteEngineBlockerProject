"use strict";

var h = React.createElement;
var useState   = React.useState;
var useEffect  = React.useEffect;
var useRef     = React.useRef;
var Fragment   = React.Fragment;

var IGNITION_ORDER   = ["off","acc","on","start"];
var THEFT_TIMEOUT_S = 60;
var MAX_ATTEMPTS     = 3;
var LOCKOUT_SECONDS  = 300;
var EVT_NAMES_MAP = {1:"STATE",2:"AUTH_FAIL",3:"AUTH_OK",4:"LOCKOUT",5:"SENSOR",6:"DERATE",7:"STARTER",8:"UNBLOCK",9:"REV_ABORT",10:"REV_EXP",11:"NVM_WR",12:"NVM_RD",13:"SIG_FLT",14:"CMD_RX",15:"SPD_STOP"};
var SCENE_W = 1280;
var SCENE_H = 720;

/**
 * @brief DJB2 hash function producing an unsigned 32-bit result.
 *
 * The unsigned right-shift by zero forces the accumulator to stay within
 * the uint32 range on each iteration, matching the C backend's hash contract.
 *
 * @param {string} str  Input string to hash.
 * @return {number}     Unsigned 32-bit hash value.
 */
function hashPassword(str) {
    var h = 5381;
    for (var i = 0; i < str.length; i++) {
        h = ((h << 5) + h + str.charCodeAt(i)) >>> 0;
    }
    return h;
}

/**
 * @brief Analogue gauge rendered as an inline SVG.
 *
 * The arc sweeps from START (220°) through SWEEP (280°). Major and minor
 * tick marks are placed at configurable intervals; labels are drawn at the
 * major positions. An optional red-zone arc is overlaid on the outer ring.
 * The needle is a tapered polygon whose tip always points to the clamped
 * value angle.
 *
 * @param {object} props
 * @param {number} props.value        Current reading; clamped to [0, maxValue].
 * @param {number} props.maxValue     Full-scale value.
 * @param {string} props.unit         Unit label rendered below the centre.
 * @param {object} props.ticks        { step, major } tick spacing.
 * @param {Array}  props.labels       Array of values at which labels are drawn.
 * @param {Array}  [props.redZone]    [start, end] values for the danger arc.
 * @param {string} props.id           Unique identifier used to namespace SVG defs.
 * @param {*}      [props.digitalValue] Overrides the digital readout string.
 */
function Gauge(props) {
  var value=props.value, maxValue=props.maxValue, unit=props.unit;
  var ticks=props.ticks, labels=props.labels, redZone=props.redZone;
  var id=props.id, digitalValue=props.digitalValue;

  var cx=50,cy=50,START=220,SWEEP=280;
  function ang(v){ return START+(v/maxValue)*SWEEP; }
  function rad(deg){ return ((deg-90)*Math.PI)/180; }
  function px(r,deg){ return cx+r*Math.cos(rad(deg)); }
  function py(r,deg){ return cy+r*Math.sin(rad(deg)); }
  function arc(r,a1,a2,sw){
    return "M "+px(r,a1)+" "+py(r,a1)+" A "+r+" "+r+" 0 "+(sw>180?1:0)+" 1 "+px(r,a2)+" "+py(r,a2);
  }
  var tickArr=[];
  for(var v=0;v<=maxValue;v+=ticks.step) tickArr.push({v:v,major:v%ticks.major===0});

  var na=ang(Math.min(value,maxValue)), nr=rad(na);
  var dx=Math.cos(nr),dy=Math.sin(nr);
  function perp(s){return {x:-dy*s,y:dx*s};}
  var tip={x:cx+dx*36,y:cy+dy*36}, tail={x:cx-dx*9,y:cy-dy*9};
  var b=perp(1.1),t=perp(0.4);
  var npts=[
    (tip.x+t.x)+","+(tip.y+t.y),
    (cx+b.x)+","+(cy+b.y),
    (tail.x+b.x)+","+(tail.y+b.y),
    (tail.x-b.x)+","+(tail.y-b.y),
    (cx-b.x)+","+(cy-b.y),
    (tip.x-t.x)+","+(tip.y-t.y)
  ].join(" ");

  var rzA1=redZone?ang(redZone[0]):0, rzA2=redZone?ang(redZone[1]):0;
  var disp=digitalValue!=null?digitalValue:value.toFixed(0);

  return h("svg",{viewBox:"0 0 100 100",style:{width:"100%",height:"100%",display:"block"}},
    h("defs",null,
      h("radialGradient",{id:"c"+id,cx:"35%",cy:"25%",r:"65%"},
        h("stop",{offset:"0%",stopColor:"#e0e0e0"}),h("stop",{offset:"45%",stopColor:"#aaa"}),h("stop",{offset:"100%",stopColor:"#444"})),
      h("radialGradient",{id:"f"+id,cx:"50%",cy:"35%",r:"70%"},
        h("stop",{offset:"0%",stopColor:"#1c1c1c"}),h("stop",{offset:"100%",stopColor:"#030303"})),
      h("filter",{id:"sh"+id},h("feDropShadow",{dx:"0",dy:"0",stdDeviation:"0.5",floodColor:"#f00",floodOpacity:"0.4"}))),
    h("circle",{cx:cx,cy:cy,r:"49",fill:"url(#c"+id+")"}),
    h("circle",{cx:cx,cy:cy,r:"46",fill:"#111"}),
    h("circle",{cx:cx,cy:cy,r:"44",fill:"url(#f"+id+")"}),
    h("circle",{cx:cx,cy:cy,r:"44",fill:"none",stroke:"#333",strokeWidth:"0.5"}),
    redZone&&h(Fragment,null,
      h("path",{d:arc(41,rzA1,rzA2,rzA2-rzA1),fill:"none",stroke:"#cc0000",strokeWidth:"3.5",strokeLinecap:"round"}),
      h("path",{d:arc(41,rzA1,rzA2,rzA2-rzA1),fill:"none",stroke:"#ff2200",strokeWidth:"1.5",strokeLinecap:"round",opacity:"0.6"})),
    h("path",{d:arc(41,START,START+SWEEP,SWEEP),fill:"none",stroke:"#1a1a1a",strokeWidth:"1"}),
    tickArr.map(function(tk){
      var a=ang(tk.v);
      return h("line",{key:tk.v,x1:px(41,a),y1:py(41,a),x2:px(tk.major?34:38,a),y2:py(tk.major?34:38,a),
        stroke:redZone&&tk.v>=redZone[0]?"#cc2200":"#fff",strokeWidth:tk.major?0.9:0.5,strokeLinecap:"round"});
    }),
    labels.map(function(v){
      var a=ang(v);
      return h("text",{key:v,x:px(27,a),y:py(27,a),textAnchor:"middle",dominantBaseline:"central",
        fill:redZone&&v>=redZone[0]?"#f55":"#ddd",fontSize:"5.8",fontFamily:"Arial,sans-serif",fontWeight:"bold"},v);
    }),
    h("text",{x:cx,y:cy+12,textAnchor:"middle",fill:"#888",fontSize:"4.5",fontFamily:"Arial,sans-serif"},unit),
    h("rect",{x:cx-13,y:cy+18,width:"26",height:"9",rx:"1",fill:"#060606",stroke:"#2a2a2a",strokeWidth:"0.5"}),
    h("text",{x:cx,y:cy+24,textAnchor:"middle",dominantBaseline:"central",
      fill:"#e8e8e8",fontSize:"5.5",fontFamily:"'Courier New',monospace",letterSpacing:"1"},disp),
    h("polygon",{points:npts,fill:"#cc0000",filter:"url(#sh"+id+")"}),
    h("line",{x1:cx+dx*2,y1:cy+dy*2,x2:tip.x,y2:tip.y,stroke:"#ff4444",strokeWidth:"0.4",strokeLinecap:"round",opacity:"0.6"}),
    h("circle",{cx:cx,cy:cy,r:"5.5",fill:"#1a1a1a",stroke:"#999",strokeWidth:"1"}),
    h("circle",{cx:cx,cy:cy,r:"2.5",fill:"#222",stroke:"#bbb",strokeWidth:"0.5"})
  );
}

/** @brief Angular positions and labels for each ignition switch detent. */
var IGN_POS=[
  {state:"off",  label:"LOCK",  angle:210},
  {state:"acc",  label:"ACC",   angle:290},
  {state:"on",   label:"ON",    angle:340},
  {state:"start",label:"START", angle:30 }
];

/**
 * @brief Rotary ignition switch rendered as an SVG widget.
 *
 * Left-click advances to the next detent; right-click (context menu) steps
 * back. The active detent label is highlighted and a radial indicator line
 * is drawn toward the current position.
 *
 * @param {object}   props
 * @param {string}   props.state   Active ignition state key from IGNITION_ORDER.
 * @param {Function} props.onNext  Called when the user clicks to advance.
 * @param {Function} props.onPrev  Called when the user right-clicks to retreat.
 */
function IgnitionKey(props){
  var state=props.state,onNext=props.onNext,onPrev=props.onPrev;
  var cx=21,cy=21;
  function toRad(deg){return ((deg-90)*Math.PI)/180;}
  var cur=IGN_POS.find(function(p){return p.state===state;}).angle;
  var cos=Math.cos(toRad(cur)),sin=Math.sin(toRad(cur));
  var perp={x:-sin,y:cos};
  var tip={x:cx+cos*7.5,y:cy+sin*7.5},base={x:cx-cos*2.5,y:cy-sin*2.5};
  var w1=0.45,w2=1.1;
  var kpts=[
    (tip.x+perp.x*w1)+","+(tip.y+perp.y*w1),
    (base.x+perp.x*w2)+","+(base.y+perp.y*w2),
    (base.x-perp.x*w2)+","+(base.y-perp.y*w2),
    (tip.x-perp.x*w1)+","+(tip.y-perp.y*w1)
  ].join(" ");
  return h("svg",{viewBox:"0 0 42 42",style:{width:"100%",height:"100%",display:"block",cursor:"pointer"},
    onClick:onNext,onContextMenu:function(e){e.preventDefault();onPrev();}},
    h("defs",null,
      h("radialGradient",{id:"igo",cx:"40%",cy:"30%",r:"70%"},h("stop",{offset:"0%",stopColor:"#3c3c3c"}),h("stop",{offset:"55%",stopColor:"#181818"}),h("stop",{offset:"100%",stopColor:"#070707"})),
      h("radialGradient",{id:"igr",cx:"50%",cy:"30%",r:"80%"},h("stop",{offset:"0%",stopColor:"#282828"}),h("stop",{offset:"100%",stopColor:"#0e0e0e"})),
      h("radialGradient",{id:"igc",cx:"35%",cy:"25%",r:"65%"},h("stop",{offset:"0%",stopColor:"#d8d8d8"}),h("stop",{offset:"40%",stopColor:"#909090"}),h("stop",{offset:"100%",stopColor:"#3c3c3c"})),
      h("filter",{id:"igg",x:"-50%",y:"-50%",width:"200%",height:"200%"},h("feDropShadow",{dx:"0",dy:"0",stdDeviation:"1.2",floodColor:"#00aaff",floodOpacity:"0.9"})),
      h("filter",{id:"igt",x:"-100%",y:"-100%",width:"300%",height:"300%"},h("feDropShadow",{dx:"0",dy:"0",stdDeviation:"0.8",floodColor:"#00aaff",floodOpacity:"1"}))),
    h("circle",{cx:cx,cy:cy,r:"20.5",fill:"url(#igo)"}),
    h("circle",{cx:cx,cy:cy,r:"20.5",fill:"none",stroke:"#555",strokeWidth:"0.4"}),
    h("circle",{cx:cx,cy:cy,r:"17",fill:"url(#igr)"}),
    h("circle",{cx:cx,cy:cy,r:"11.5",fill:"#060606"}),
    IGN_POS.map(function(p){
      var r=14.3,rd=toRad(p.angle),x=cx+r*Math.cos(rd),y=cy+r*Math.sin(rd);
      return h("text",{key:p.state,x:x,y:y,textAnchor:"middle",dominantBaseline:"central",
        fontSize:p.label.length>3?"2.1":"2.5",fontFamily:"Arial,sans-serif",fontWeight:"bold",letterSpacing:"0.2",
        fill:p.state===state?"#33ccff":"#555",filter:p.state===state?"url(#igg)":undefined},p.label);
    }),
    h("line",{x1:cx+Math.cos(toRad(cur))*11.5,y1:cy+Math.sin(toRad(cur))*11.5,
      x2:cx+Math.cos(toRad(cur))*13.5,y2:cy+Math.sin(toRad(cur))*13.5,
      stroke:"#33ccff",strokeWidth:"1.2",strokeLinecap:"round",filter:"url(#igt)"}),
    h("circle",{cx:cx,cy:cy,r:"9.5",fill:"url(#igc)"}),
    h("circle",{cx:cx,cy:cy,r:"9.5",fill:"none",stroke:"#bbb",strokeWidth:"0.5"}),
    h("polygon",{points:kpts,fill:"#111",stroke:"#aaa",strokeWidth:"0.35"}),
    h("ellipse",{cx:cx,cy:cy,rx:"2.2",ry:"2.2",fill:"#0a0a0a",stroke:"#aaa",strokeWidth:"0.35"})
  );
}


/**
 * @brief Animated perspective road scene rendered on a canvas element.
 *
 * A rAF loop updates @c posRef by @c speed * dt * 2.6 each frame, which
 * drives the phase offset of the dashed lane markings and guard-rail posts.
 * The dash phase formula uses a perspective-depth variable d = 1/(u + ε)
 * so that markings scroll faster near the camera and slower at the horizon.
 * Guard-rail posts use the same depth space and are sorted back-to-front
 * (painter's algorithm) before rasterisation.
 *
 * @param {object} props
 * @param {number} props.speed  Vehicle speed in km/h; drives scroll rate.
 */
function WindshieldView(props){
  var speed = props.speed;
  var spRef = useRef(speed);
  useEffect(function(){ spRef.current = speed; }, [speed]);

  var canvasRef  = useRef(null);
  var posRef     = useRef(0);
  var lastTRef   = useRef(null);
  var rafRef     = useRef(null);

  useEffect(function(){
    var canvas = canvasRef.current;
    if(!canvas) return;
    var ctx = canvas.getContext('2d');

    var W  = 1280, H = 800;
    var HY = 560;

    var rHLx = 605, rHRx = 675;
    var rBLx = 120, rBRx = 1160;

    var gLHx = 596, gLBx =  98;
    var gRHx = 684, gRBx = 1182;

    function lerp(a, b, t){ return a + (b - a) * t; }

    function quadSeg(x0, y0, w0, x1, y1, w1, color){
      ctx.beginPath();
      ctx.moveTo(x0 - w0/2, y0); ctx.lineTo(x0 + w0/2, y0);
      ctx.lineTo(x1 + w1/2, y1); ctx.lineTo(x1 - w1/2, y1);
      ctx.closePath();
      ctx.fillStyle = color;
      ctx.fill();
    }

    function drawSolid(hx, bx, color){
      var SEGS = 80;
      for(var i = 0; i < SEGS; i++){
        var t0 = i / SEGS, t1 = (i+1) / SEGS;
        quadSeg(
          lerp(hx,bx,t0), HY + t0*(H-HY), t0*3.5 + 0.4,
          lerp(hx,bx,t1), HY + t1*(H-HY), t1*3.5 + 0.4,
          color
        );
      }
    }

    /**
     * @brief Draws a perspective-correct animated dashed line.
     *
     * Depth d = 1/(u + ε). The phase (d + tOff) % PERIOD < DASH_F * PERIOD
     * determines whether each integration step is inside a dash segment.
     * As tOff grows the dashes migrate from horizon toward the camera,
     * with on-screen speed proportional to u².
     *
     * @param {number} hx    Horizon x-coordinate.
     * @param {number} bx    Base x-coordinate.
     * @param {string} color Fill colour.
     * @param {number} thk   Width scale multiplier.
     */
    function drawDash(hx, bx, color, thk){
      var eps    = 0.014;
      var PERIOD = 0.60;
      var DASH_F = 0.44;
      var STEPS  = 380;
      var ts     = thk || 1;

      var tOff = posRef.current * 0.012;

      var inDash = false, py = 0, px2 = 0, pu = 0;

      for(var i = 1; i <= STEPS; i++){
        var u  = i / STEPS;
        var y  = HY + u * (H - HY);
        var x  = lerp(hx, bx, u);
        var d  = 1 / (u + eps);
        var on = (d + tOff) % PERIOD < DASH_F * PERIOD;

        if(on && !inDash){ py = y; px2 = x; pu = u; }
        else if(!on && inDash){
          quadSeg(px2, py, (pu*3.8+0.4)*ts, x, y, (u*3.8+0.4)*ts, color);
        }
        if(on) pu = u;
        inDash = on;
      }
      if(inDash){
        quadSeg(px2, py, (pu*3.8+0.4)*ts, lerp(hx,bx,1), H, 4.2*ts, color);
      }
    }

    /**
     * @brief Draws an animated guard-rail (W-beam + posts) along a perspective edge.
     *
     * Post positions are computed in the same depth space as drawDash so that
     * posts and lane markings scroll at identical rates. Posts are collected,
     * sorted by ascending u (horizon to camera), and painted back-to-front.
     *
     * @param {number} hx  Horizon x-coordinate of the rail.
     * @param {number} bx  Base x-coordinate of the rail.
     */
    function drawRail(hx, bx){
      var SEGS = 80;
      for(var beam = 0; beam < 2; beam++){
        var bf = 0.28 + beam * 0.20;
        for(var s = 0; s < SEGS; s++){
          var t0 = s / SEGS, t1 = (s+1) / SEGS;
          var ph0 = t0*26 + 2, ph1 = t1*26 + 2;
          var bh0 = ph0 * 0.13, bh1 = ph1 * 0.13;
          var y0  = HY + t0*(H-HY), y1 = HY + t1*(H-HY);
          var x0  = lerp(hx, bx, t0), x1 = lerp(hx, bx, t1);
          quadSeg(
            x0, y0 - ph0*bf, bh0*3.5,
            x1, y1 - ph1*bf, bh1*3.5,
            'rgba(155,162,168,0.92)'
          );
        }
      }

      var eps_p  = 0.014;
      var POST_D = 1.70;
      var tOff   = posRef.current * 0.012;
      var D_MIN  = 1.0 / (1.0 + eps_p);
      var D_MAX  = 1.0 / eps_p;
      var k_min  = Math.ceil ((D_MIN + tOff) / POST_D);
      var k_max  = Math.floor((D_MAX + tOff) / POST_D);

      var postsU = [];
      for(var k = k_min; k <= k_max; k++){
        var d = k * POST_D - tOff;
        if(d <= 0) continue;
        var u = 1.0 / d - eps_p;
        if(u >= 0 && u <= 1) postsU.push(u);
      }
      postsU.sort(function(a, b){ return a - b; });

      postsU.forEach(function(t){
        var y  = HY + t * (H - HY);
        var x  = lerp(hx, bx, t);
        var pw = t * 8  + 1.2;
        var ph = t * 26 + 2.0;
  
        ctx.fillStyle = '#a1a6ac';
        ctx.fillRect(x - pw/2, y - ph, pw, ph);
        ctx.fillStyle = '#797875';
        ctx.fillRect(x - pw/2, y - ph, pw * 0.38, ph);
      });
  
    }

    function drawFrame(now){
      var dt = lastTRef.current
        ? Math.min((now - lastTRef.current) / 1000, 0.05)
        : 0.016;
      lastTRef.current = now;
      posRef.current  += spRef.current * dt * 2.6;

      ctx.clearRect(0, 0, W, H);

      var farG = ctx.createLinearGradient(0, HY, 0, HY+55);
      farG.addColorStop(0, '#88b848');
      farG.addColorStop(1, '#68982e');
      ctx.fillStyle = farG;
      ctx.fillRect(0, HY, W, 55);

      var lgG = ctx.createLinearGradient(0, HY, 0, H);
      lgG.addColorStop(0, '#88b848');
      lgG.addColorStop(0.38, '#68982e');
      lgG.addColorStop(1, '#487818');
      ctx.beginPath();
      ctx.moveTo(0, HY); ctx.lineTo(gLHx, HY);
      ctx.lineTo(gLBx, H); ctx.lineTo(0, H);
      ctx.closePath();
      ctx.fillStyle = lgG; ctx.fill();

      ctx.beginPath();
      ctx.moveTo(W, HY); ctx.lineTo(gRHx, HY);
      ctx.lineTo(gRBx, H); ctx.lineTo(W, H);
      ctx.closePath();
      ctx.fillStyle = lgG; ctx.fill();

      ctx.beginPath();
      ctx.moveTo(gLHx, HY); ctx.lineTo(rHLx, HY);
      ctx.lineTo(rBLx,  H); ctx.lineTo(gLBx,  H);
      ctx.closePath();
      ctx.fillStyle = '#686868';
      ctx.fill();

      ctx.beginPath();
      ctx.moveTo(rHRx, HY); ctx.lineTo(gRHx, HY);
      ctx.lineTo(gRBx,  H); ctx.lineTo(rBRx,  H);
      ctx.closePath();
      ctx.fillStyle = '#686868';
      ctx.fill();

      ctx.beginPath();
      ctx.moveTo(rBLx, H); ctx.lineTo(rBRx, H);
      ctx.lineTo(rHRx, HY); ctx.lineTo(rHLx, HY);
      ctx.closePath();
      var roadG = ctx.createLinearGradient(0, HY, 0, H);
      roadG.addColorStop(0, '#686868');
      roadG.addColorStop(0.32, '#595959');
      roadG.addColorStop(1, '#484848');
      ctx.fillStyle = roadG; ctx.fill();

      ctx.beginPath();
      ctx.moveTo(rBLx, H); ctx.lineTo(rBRx, H);
      ctx.lineTo(rHRx, HY); ctx.lineTo(rHLx, HY);
      ctx.closePath();
      var glossG = ctx.createLinearGradient(0, HY, 0, H);
      glossG.addColorStop(0, 'rgba(110,120,130,0)');
      glossG.addColorStop(0.65, 'rgba(100,110,120,0.05)');
      glossG.addColorStop(1, 'rgba(88,98,110,0.13)');
      ctx.fillStyle = glossG; ctx.fill();

      drawSolid(rHLx, rBLx, 'rgba(255,255,255,0.87)');
      drawSolid(rHRx, rBRx, 'rgba(255,255,255,0.87)');

      drawDash(lerp(rHLx,rHRx,0.25), lerp(rBLx,rBRx,0.25), 'rgba(255,255,255,0.84)', 0.82);
      drawDash(lerp(rHLx,rHRx,0.75), lerp(rBLx,rBRx,0.75), 'rgba(255,255,255,0.84)', 0.82);
      drawSolid(lerp(rHLx,rHRx,0.5)-0.6, lerp(rBLx,rBRx,0.5)-2.0, 'rgba(255,198,0,0.92)');
      drawSolid(lerp(rHLx,rHRx,0.5)+0.6, lerp(rBLx,rBRx,0.5)+2.0, 'rgba(255,198,0,0.92)');

      drawRail(gLHx, gLBx);
      drawRail(gRHx, gRBx);

      var vigG = ctx.createRadialGradient(640, 660, 80, 640, 660, 720);
      vigG.addColorStop(0, 'rgba(0,0,0,0)');
      vigG.addColorStop(1, 'rgba(0,0,0,0.22)');
      ctx.fillStyle = vigG;
      ctx.fillRect(0, HY, W, H - HY);

      rafRef.current = requestAnimationFrame(drawFrame);
    }

    rafRef.current = requestAnimationFrame(drawFrame);
    return function(){
      if(rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, []);

  return h('canvas', {
    ref:   canvasRef,
    width: 1280,
    height:800,
    style: { width:'100%', height:'100%', display:'block' }
  });
}


/** @brief Monotonic entry ID counter for smartphone log entries. */
var _eid=0;
function ts(){return new Date().toLocaleTimeString();}
function mkEntry(emoji,text,type){return {id:++_eid,emoji:emoji,text:text,time:ts(),type:type};}

/** @brief Colour map keyed by log entry type string. */
var EC={alert:"#ff4444",warn:"#ff9900",success:"#00cc66",location:"#33aaff",sent:"#ffaa00",info:"#66aacc"};

/**
 * @brief Smartphone UI panel showing a system message log and a contextual
 *        command area driven by phoneFlow state.
 *
 * The command area renders one of five sub-views based on phoneFlow:
 * "theft_question", "blocking_pending", "unlocking_wait",
 * "blocked_confirm", or idle (default remote lock prompt).
 *
 * @param {object}   props
 * @param {Array}    props.phoneLog        Array of log entry objects.
 * @param {string}   props.phoneFlow       Active flow state string.
 * @param {number}   props.theftCd         Theft auto-confirm countdown in seconds.
 * @param {Function} props.onTheftSim      User confirmed auto-block after theft alert.
 * @param {Function} props.onTheftNo       User dismissed theft alert.
 * @param {Function} props.onLockSim       User requested remote lock.
 * @param {Function} props.onLockNo        User dismissed lock prompt.
 * @param {Function} props.onUnlockSim     User requested remote unlock.
 * @param {Function} props.onUnlockNo      User dismissed unlock prompt.
 */
function SmartphonePanel(props){
  var phoneLog   =props.phoneLog,   phoneFlow  =props.phoneFlow;
  var theftCd    =props.theftCd,    onTheftSim =props.onTheftSim;
  var onTheftNo  =props.onTheftNo,  onLockSim  =props.onLockSim;
  var onLockNo   =props.onLockNo,   onUnlockSim=props.onUnlockSim;
  var onUnlockNo =props.onUnlockNo;

  var W=246,H=359,fR=22,pad=7,notchW=62,notchH=14;
  var sw=W-pad*2,sh=H-pad*2,stH=20,btH=14;
  var msgH=Math.floor((sh-stH-btH)*0.55);
  var now=new Date().toLocaleTimeString([],{hour:"2-digit",minute:"2-digit"});
  function bs(bg,bd,c){return {flex:1,padding:"5px 4px",background:bg,border:"1px solid "+bd,borderRadius:6,color:c,fontSize:9,fontFamily:"'Courier New',monospace",fontWeight:"bold",cursor:"pointer",lineHeight:1.2,textAlign:"center"};}

  var cmdArea;
  if(phoneFlow==="theft_question"){
    cmdArea=h(Fragment,null,
      h("div",{style:{fontSize:8,color:"#f44",lineHeight:1.4,fontWeight:"bold"}},"Theft attempt detected on your vehicle. Confirm?"),
      h("div",{style:{fontSize:7,color:"#f80",marginTop:2,marginBottom:3}},"Auto-block in: "+theftCd+"s"),
      h("div",{style:{display:"flex",gap:4}},
        h("button",{style:bs("#0a1a00","#0f0","#0f0"),onClick:onTheftSim},"Yes"),
        h("button",{style:bs("#1a0000","#f00","#f00"),onClick:onTheftNo},"No")));
  } else if(phoneFlow==="blocking_pending"){
    cmdArea=h("div",{style:{flex:1,display:"flex",alignItems:"center",justifyContent:"center",textAlign:"center",padding:"4px"}},
      h("div",{style:{fontSize:8,color:"#f80",lineHeight:1.6,fontWeight:"bold"}},"Your vehicle has started\nthe blocking process"));
  } else if(phoneFlow==="unlocking_wait"){
    cmdArea=h("div",{style:{flex:1,display:"flex",alignItems:"center",justifyContent:"center",textAlign:"center",padding:"4px"}},
      h("div",{style:{fontSize:8,color:"#0af",lineHeight:1.6,fontWeight:"bold"}},"Waiting for unblock\nconfirmation..."));
  } else if(phoneFlow==="blocked_confirm"){
    cmdArea=h(Fragment,null,
      h("div",{style:{fontSize:8,color:"#0f0",lineHeight:1.4,fontWeight:"bold",marginBottom:2}},"Your vehicle has been successfully blocked"),
      h("div",{style:{fontSize:8,color:"#aaa",lineHeight:1.4}},"Do you want to remotely unblock the vehicle?"),
      h("div",{style:{display:"flex",gap:4,marginTop:4}},
        h("button",{style:bs("#001a08","#0f0","#0f0"),onClick:onUnlockSim},"Yes"),
        h("button",{style:bs("#0d0d0d","#333","#555"),onClick:onUnlockNo},"No")));
  } else {
    cmdArea=h(Fragment,null,
      h("div",{style:{fontSize:8,color:"#aaa",lineHeight:1.4}},"Do you want to remotely block the vehicle?"),
      h("div",{style:{display:"flex",gap:4,marginTop:4}},
        h("button",{style:bs("#0d0000","#f44","#f44"),onClick:onLockSim},"Yes"),
        h("button",{style:bs("#0d0d0d","#333","#555"),onClick:onLockNo},"No")));
  }

  return h("div",{style:{position:"relative",width:W,height:H,pointerEvents:"auto"}},
    h("svg",{style:{position:"absolute",inset:0,width:"100%",height:"100%",overflow:"visible"},viewBox:"0 0 "+W+" "+H},
      h("defs",null,
        h("linearGradient",{id:"psh",x1:"0%",y1:"0%",x2:"100%",y2:"100%"},h("stop",{offset:"0%",stopColor:"#2a2a2a"}),h("stop",{offset:"40%",stopColor:"#111"}),h("stop",{offset:"100%",stopColor:"#050505"})),
        h("linearGradient",{id:"ped",x1:"0%",y1:"0%",x2:"100%",y2:"0%"},h("stop",{offset:"0%",stopColor:"#444"}),h("stop",{offset:"50%",stopColor:"#888"}),h("stop",{offset:"100%",stopColor:"#333"}))),
      h("rect",{x:0,y:0,width:W,height:H,rx:fR,ry:fR,fill:"url(#psh)"}),
      h("rect",{x:.5,y:.5,width:W-1,height:H-1,rx:fR-.5,ry:fR-.5,fill:"none",stroke:"url(#ped)",strokeWidth:"1.2"}),
      h("rect",{x:pad,y:pad,width:sw,height:sh,rx:fR-pad,ry:fR-pad,fill:"#08080f"}),
      h("rect",{x:(W-notchW)/2,y:pad-1,width:notchW,height:notchH,rx:notchH/2,ry:notchH/2,fill:"#111"}),
      h("rect",{x:W-3,y:H*.28,width:3,height:28,rx:"1.5",fill:"#222"}),
      h("rect",{x:W-3,y:H*.42,width:3,height:42,rx:"1.5",fill:"#222"}),
      h("rect",{x:0,y:H*.32,width:3,height:22,rx:"1.5",fill:"#222"}),
      h("rect",{x:0,y:H*.44,width:3,height:22,rx:"1.5",fill:"#222"})),
    h("div",{style:{position:"absolute",left:pad,top:pad,width:sw,height:sh,borderRadius:fR-pad,overflow:"hidden",background:"#08080f",fontFamily:"'Courier New',monospace",display:"flex",flexDirection:"column"}},
      h("div",{style:{height:stH,background:"#050510",display:"flex",alignItems:"center",justifyContent:"space-between",padding:"0 10px",paddingTop:notchH-4,flexShrink:0}},
        h("span",{style:{fontSize:7.5,color:"#666"}},now),
        h("div",{style:{display:"flex",gap:3,alignItems:"center"}},
          [3,4,5,5].map(function(ht,i){return h("div",{key:i,style:{width:2.5,height:ht,background:i<3?"#0af":"#222",borderRadius:1}});}),
          h("span",{style:{fontSize:6,color:"#555",marginLeft:2}},"4G"))),
      h("div",{style:{height:msgH,overflowY:"auto",background:"#06060c",flexShrink:0,padding:"4px 0"}},
        h("div",{style:{fontSize:7.5,color:"#0af",fontWeight:"bold",padding:"0 8px 3px",borderBottom:"0.5px solid rgba(0,170,255,.1)",marginBottom:2,letterSpacing:.5}},"SYSTEM MESSAGES"),
        phoneLog.length===0&&h("div",{style:{fontSize:8,color:"#222",padding:"4px 8px",fontStyle:"italic"}},"No messages"),
        phoneLog.map(function(e){return h("div",{key:e.id,style:{padding:"3px 8px",borderBottom:"0.5px solid #0a0a15",display:"flex",gap:4,alignItems:"flex-start"}},
          h("span",{style:{fontSize:9,flexShrink:0,marginTop:1}},e.emoji),
          h("div",{style:{flex:1,minWidth:0}},
            h("div",{style:{fontSize:8,color:EC[e.type]||"#66aacc",lineHeight:1.35,wordBreak:"break-word"}},e.text),
            h("div",{style:{fontSize:6.5,color:"#333",marginTop:1}},e.time)));})),
      h("div",{style:{height:1,background:"linear-gradient(90deg,transparent,rgba(0,170,255,.25),transparent)",flexShrink:0}}),
      h("div",{style:{flex:1,background:"#05050e",padding:"6px 8px",display:"flex",flexDirection:"column",gap:5,overflow:"hidden"}},cmdArea),
      h("div",{style:{height:btH,background:"#030308",display:"flex",alignItems:"center",justifyContent:"center",flexShrink:0}},
        h("div",{style:{width:36,height:4,borderRadius:2,background:"#222"}})))
  );
}

/**
 * @brief Root cockpit component managing all simulation state and UI rendering.
 *
 * Responsibilities:
 *  - Scales the fixed 1280×720 scene to fit the viewport via a CSS transform.
 *  - Maintains the REB backend output snapshot (rebOut) and merges patches via apiStep.
 *  - Drives the ignition switch, pedal inputs, window/sensor fusion events, and
 *    panel authentication flow.
 *  - Polls /api/snapshot every second and reacts to backend state transitions to
 *    advance the smartphone flow (phoneFlow) and infotainment sub-state (p1Sub).
 *  - Synchronises input state with the Technical Interface tab via localStorage.
 *  - Provides a turbo mode that issues repeated step calls at a configurable cycle
 *    multiplier to accelerate the simulation.
 */
function ControlInterface(){
  var scaleState=useState(1); var scale=scaleState[0],setScale=scaleState[1];
  var offXState=useState(0);  var offX=offXState[0], setOffX=offXState[1];
  var offYState=useState(0);  var offY=offYState[0], setOffY=offYState[1];
  var zoomState=useState(null); var zoom=zoomState[0],setZoom=zoomState[1];

  useEffect(function(){
    function upd(){
      var tabH=37,avH=window.innerHeight-tabH;
      var s=Math.max(window.innerWidth/SCENE_W,avH/SCENE_H);
      setScale(s);setOffX((window.innerWidth-SCENE_W*s)/2);setOffY((avH-SCENE_H*s)/2);
    }
    upd();window.addEventListener("resize",upd);return function(){window.removeEventListener("resize",upd);};
  },[]);

  var ignIdxState=useState(0); var ignIdx=ignIdxState[0],setIgnIdx=ignIdxState[1];
  var ignState=IGNITION_ORDER[ignIdx];

  /**
   * @brief baseSpeed holds raw pedal-driven speed; speed is the derated display value.
   *
   * effective speed = baseSpeed * derate_pct / 100. When starter_inhibit_active
   * is true speed is forced to zero regardless of baseSpeed.
   */
  var baseSpeedState=useState(0); var baseSpeed=baseSpeedState[0],setBaseSpeed=baseSpeedState[1];
  var speedState=useState(0); var speed=speedState[0],setSpeed=speedState[1];
  var rpmState=useState(800); var rpm=rpmState[0],setRpm=rpmState[1];
  /* rpm derived from speed via physical model:
   * RPM = (speed_kmh * gear_ratio * diff_ratio * 1000) / (wheel_radius_m * 2 * PI * 60)
   * wheel radius 0.315 m (195/65 R15), diff 3.9:1, top gear 0.72:1 (6th)
   * => idle 800 rpm, ~2100 rpm @ 120 km/h, ~4500 rpm @ 260 km/h */
  function speedToRpm(spd, ignSt){
    if(ignSt==="off"||ignSt==="acc") return 0;
    if(spd===0) return 800;
    var WHEEL_R = 0.315, DIFF = 3.9, GEAR = 0.72;
    var raw = (spd * GEAR * DIFF * 1000) / (WHEEL_R * 2 * Math.PI * 60);
    return Math.round(Math.max(800, Math.min(8000, raw)));
  }

  var rebOutState=useState({state:"IDLE",derate_pct:100,starter_ok:true,
    starter_inhibit_active:false,alert_visual:false,alert_sonic:false,
    notify_theft:false,notify_blocked:false,blocked_flag:false,
    powertrain_valid:true,channel_rx_ok:true,
    anti_replay:{ok:true,reason:"AUTH_OK",last_nonce:0},
    sensor_score:0,parked_timer_s:0,reversal_timer_s:0,
    panel_attempt_count:0,panel_locked_out:false,panel_password_ok:false,
    logs:[],sim_time_ms:0});
  var rebOut=rebOutState[0],setRebOut=rebOutState[1];

  var pageState=useState(1); var page=pageState[0],setPage=pageState[1];

  var turboMultState=useState(1); var turboMult=turboMultState[0],setTurboMult=turboMultState[1];
  var turboRunState=useState(false); var turboRun=turboRunState[0],setTurboRun=turboRunState[1];
  var turboRef=useRef(null);
  var turboMultRef=useRef(1);
  var prevPanelLockedRef = useRef(false);
  useEffect(function(){turboMultRef.current=turboMult;},[turboMult]);

  /**
   * @brief Interval refs for accelerator (aI) and brake (bI) pedal hold-repeat.
   *
   * Pressing a pedal starts an interval that continuously increments or
   * decrements baseSpeed and rpm. Releasing clears the interval.
   * The accelerator is suppressed when starter_inhibit_active is true or
   * ignition is not in the "start" position.
   */
  var aI=useRef(null),bI=useRef(null);
  function sA(){aI.current=setInterval(function(){setBaseSpeed(function(s){return Math.min(260,s+2);});},80);}
  function eA(){clearInterval(aI.current);}
  function sB(){bI.current=setInterval(function(){setBaseSpeed(function(s){return Math.max(0,s-3);});},80);}
  function eB(){clearInterval(bI.current);}
  useEffect(function(){return function(){clearInterval(aI.current);clearInterval(bI.current);};},[]);
  var apState=useState(false); var apOn=apState[0],setApOn=apState[1];
  var bpState=useState(false); var bpOn=bpState[0],setBpOn=bpState[1];
  function hAD(){
    if(rebOut.starter_inhibit_active)return;
    if(ignState!=="start")return;
    setApOn(true);sA();
  }
  function hAU(){setApOn(false);eA();}
  function hBD(){
    if(rebOut.starter_inhibit_active)return;
    setBpOn(true);sB();
  }
  function hBU(){setBpOn(false);eB();}

  /**
   * @brief Derives the effective display speed from baseSpeed and the current derate_pct.
   *
   * This is the single authoritative location where both the speed state and the
   * vehicle_speed_kmh input field in currentInputsRef are updated. All other
   * code reads speed (state) or currentInputsRef, never baseSpeed directly.
   */
  useEffect(function(){
    var blocked = !!rebOut.starter_inhibit_active;
    var eff;
    if(blocked){
      eff = 0;
      setRpm(0);
    } else {
      var derate = typeof rebOut.derate_pct === "number" ? rebOut.derate_pct : 100;
      eff = Math.round(baseSpeed * derate / 100);
      setRpm(speedToRpm(eff, ignState));
    }
    setSpeed(eff);
    currentInputsRef.current.vehicle_speed_kmh = eff;
  },[baseSpeed, rebOut.derate_pct, rebOut.starter_inhibit_active, ignState]);

  /**
   * @brief Stops all pedal activity and zeroes baseSpeed when starter_inhibit_active
   *        transitions to true.
   */
  useEffect(function(){
    if(rebOut.starter_inhibit_active){
      eA(); eB();
      setApOn(false); setBpOn(false);
      setBaseSpeed(0);
    }
  },[rebOut.starter_inhibit_active]);

  var fWinState=useState("normal"); var fWin=fWinState[0],setFWin=fWinState[1];
  var sWinState=useState("normal"); var sWin=sWinState[0],setSWin=sWinState[1];
  var aPkState=useState(0); var aPk=aPkState[0],setAPk=aPkState[1];
  var gPkState=useState(0); var gPk=gPkState[0],setGPk=gPkState[1];
  var crackState=useState({x:340,y:120}); var crack=crackState[0],setCrack=crackState[1];

  var sLogState=useState([]); var sLog=sLogState[0],setSLog=sLogState[1];
  var sfOnState=useState(false); var sfOn=sfOnState[0],setSfOn=sfOnState[1];
  var lkdState=useState(false); var lkd=lkdState[0],setLkd=lkdState[1];

  var pwState=useState(""); var pw=pwState[0],setPw=pwState[1];
  var pwStState=useState("idle"); var pwSt=pwStState[0],setPwSt=pwStState[1];
  var attState=useState(0); var att=attState[0],setAtt=attState[1];
  var lkUntilState=useState(null); var lkUntil=lkUntilState[0],setLkUntil=lkUntilState[1];
  var lkCdState=useState(0); var lkCd=lkCdState[0],setLkCd=lkCdState[1];

  /** @brief Tick counter incremented by the storage event handler to trigger re-renders. */
  var syncTickState=useState(0); var syncTick=syncTickState[0],setSyncTick=syncTickState[1];

  /**
   * @brief Infotainment panel page-1 sub-state machine.
   *
   * Valid values: "home_block" | "home_unblock" | "pw_entry" |
   * "blocking_progress" | "unblocking_progress" | "unblocked_done".
   * Transitions are driven by backend state changes and user input.
   */
  var p1SubState=useState("home_block"); var p1Sub=p1SubState[0],setP1Sub=p1SubState[1];
  var p1IntentState=useState("block");   var p1Intent=p1IntentState[0],setP1Intent=p1IntentState[1];


  /**
   * @brief Advances p1Sub when the backend FSM reaches BLOCKED or returns to IDLE.
   *
   * Transitions are conditional on the current sub-state to avoid overriding
   * user-initiated flows that are already further along.
   */
  useEffect(function(){
    var st = rebOut.state || "IDLE";
    if(st === "BLOCKED"){
      setP1Sub(function(s){
        if(s==="blocking_progress"||s==="home_block") return "home_unblock";
        return s;
      });
    }
    if(st === "IDLE"){
      setP1Sub(function(s){
        if(s==="unblocking_progress") return "unblocked_done";
        if(s==="home_unblock")        return "home_block";
        if(s==="blocking_progress")   return "home_block";
        return s;
      });
    }
  },[rebOut.state]);

  /** @brief Auto-returns p1Sub from "unblocked_done" to "home_block" after 10 seconds. */
  useEffect(function(){
    if(p1Sub!=="unblocked_done")return;
    var t=setTimeout(function(){setP1Sub("home_block");},10000);
    return function(){clearTimeout(t);};
  },[p1Sub]);

  /** @brief Counts down lkCd while a panel lockout is active. Clears state on expiry. */
  useEffect(function(){
    if(!lkUntil)return;
    var t=setInterval(function(){
      var rem=Math.ceil((lkUntil-Date.now())/1000);
      if(rem<=0){setLkUntil(null);setAtt(0);setLkCd(0);}else setLkCd(rem);
    },500);
    return function(){clearInterval(t);};
  },[lkUntil]);

  /**
   * @brief Accumulated input object sent to /api/step on every call to apiStep.
   *
   * Initialised from a localStorage snapshot written by the Technical Interface
   * tab when present; otherwise defaults to the BridgeInputs zero-state. Persisted
   * back to localStorage after every step so both tabs remain in sync.
   */
  var _ciDefaults={
    vehicle_speed_kmh:0.0,engine_rpm:0,ignition_state:0,brake_pedal:0.0,
    speed_sig_status:0,accel_peak:0.0,glass_break_flag:0.0,
    ip_rx_ok:true,sms_rx_ok:true,auth_blocked_remote:false,
    auth_block_automatic:0,remote_unblock_remote:false,cmd_nonce:1,
    cmd_timestamp_ms:0,cmd_sig_ok:true,tcu_ack:true,cancel_request:false,
    auth_manual_out:false,password_attempt:2088290703,sim_time_ms:0
  };
  (function(){try{var s=localStorage.getItem("reb_inputs");if(s){var p=JSON.parse(s);Object.keys(p).forEach(function(k){if(k in _ciDefaults)_ciDefaults[k]=p[k];});}}catch(e){}}());
  var currentInputsRef=useRef(_ciDefaults);

  var phoneFlowState=useState("idle");
  var phoneFlow=phoneFlowState[0],setPhoneFlow=phoneFlowState[1];

  /** @brief Ref mirror of phoneFlow for safe use inside polling and timer closures. */
  var phoneFlowRef=useRef("idle");
  useEffect(function(){phoneFlowRef.current=phoneFlow;},[phoneFlow]);
  var theftCdState=useState(THEFT_TIMEOUT_S);
  var theftCd=theftCdState[0],setTheftCd=theftCdState[1];
  var theftIntervalRef=useRef(null);
  var prevNotifyTheftRef=useRef(false);
  var prevNotifyBlockedRef=useRef(false);
  var doTheftSimRef=useRef(null);

  var pLogState=useState([mkEntry("","GPS: signal active","location")]);
  var pLog=pLogState[0],setPLog=pLogState[1];
  function addLog(e){setPLog(function(l){return [e].concat(l);});}

  /**
   * @brief Merges a patch into currentInputsRef and POSTs the full input set to /api/step.
   *
   * cmd_timestamp_ms is always overwritten with the current sim_time_ms to keep the
   * anti-replay window aligned with the simulation clock. Ephemeral flags
   * (auth_blocked_remote, auth_block_automatic, etc.) are written into a localStorage
   * pulse key before being reset, allowing the Technical Interface to react visually.
   * After the response, cmd_nonce is advanced to last_nonce + 1 and both sim_time_ms
   * and the persistent localStorage entry are updated.
   *
   * @param {object} patch   Input field overrides to apply before sending.
   * @param {string} mode    Backend operating mode string ("AUTO", "REMOTE", "MANUAL").
   * @param {number} cycles  Number of simulation cycles to execute.
   * @return {Promise}       Resolves with the backend response object.
   */
  function apiStep(patch,mode,cycles){
    var ci=currentInputsRef.current;
    if(patch){Object.keys(patch).forEach(function(k){ci[k]=patch[k];});}
    ci.cmd_timestamp_ms=ci.sim_time_ms;
    var nc=cycles||turboMultRef.current||1;
    var payload=Object.assign({},ci);
    var _ephKeys=["auth_blocked_remote","auth_block_automatic","remote_unblock_remote","auth_manual_out","cancel_request"];
    var _pulse={};
    _ephKeys.forEach(function(k){if(payload[k]&&payload[k]!==false&&payload[k]!==0)_pulse[k]=payload[k];});
    if(Object.keys(_pulse).length>0){
      _pulse.ts=Date.now();
      try{localStorage.setItem("reb_cmd_pulse",JSON.stringify(_pulse));}catch(ex){}
    }
    ci.auth_blocked_remote=false;
    ci.auth_block_automatic=0;
    ci.remote_unblock_remote=false;
    ci.auth_manual_out=false;
    ci.cancel_request=false;
    ci.password_attempt=2088290703;
    try{localStorage.setItem("reb_inputs",JSON.stringify(ci));}catch(e){}
    return fetch("/api/step",{method:"POST",headers:{"Content-Type":"application/json"},
      body:JSON.stringify({inputs:payload,mode:mode||"AUTO",cycles:nc})})
      .then(function(r){return r.json();}).then(function(d){
        if(d && typeof d === "object") setRebOut(d);
        if(d.anti_replay&&d.anti_replay.last_nonce!=null){
          ci.cmd_nonce=Number(d.anti_replay.last_nonce)+1;
          ci.cmd_timestamp_ms=d.sim_time_ms||0;
        }
        if(d.sim_time_ms!=null)ci.sim_time_ms=d.sim_time_ms;
        try{localStorage.setItem("reb_inputs",JSON.stringify(ci));}catch(e){}
        return d;
      }).catch(function(){});
  }

  /**
   * @brief Starts an interval that calls apiStep at 200 ms intervals using the
   *        current turboMult cycle count.
   */
  function startTurbo(){
    if(turboRef.current)return;
    setTurboRun(true);
    turboRef.current=setInterval(function(){
      apiStep(null,"AUTO",turboMultRef.current);
    },200);
  }
  function stopTurbo(){
    if(turboRef.current){clearInterval(turboRef.current);turboRef.current=null;}
    setTurboRun(false);
  }

  /**
   * @brief Resets the simulation to a clean initial state, including NVM erasure.
   *
   * Stops the turbo loop, POSTs to /api/reset with clear_nvm=true, then resets
   * all UI state, smartphone flow, edge-detection refs, and currentInputsRef to
   * their default values. localStorage entries are removed so the Technical
   * Interface tab also starts clean.
   */
  function doReset(){
    stopTurbo();
    fetch("/api/reset",{method:"POST",headers:{"Content-Type":"application/json"},
      body:JSON.stringify({clear_nvm:true})})
    .then(function(r){return r.json();}).then(function(d){
      if(d&&typeof d==="object") setRebOut(d);
      setP1Sub("home_block");
      setP1Intent("block");
      setPw(""); setPwSt("idle");
      setAtt(0); setLkUntil(null); setLkCd(0);
      setPhoneFlow("idle");
      setPLog([mkEntry("","GPS: signal active","location")]);
      setSLog([]);
      setSfOn(false); setLkd(false);
      prevPanelLockedRef.current   = false;
      prevNotifyTheftRef.current   = false;
      prevNotifyBlockedRef.current = false;
      currentInputsRef.current = {
        vehicle_speed_kmh:0.0, engine_rpm:0, ignition_state:0, brake_pedal:0.0,
        speed_sig_status:0, accel_peak:0.0, glass_break_flag:0.0,
        ip_rx_ok:true, sms_rx_ok:true, auth_blocked_remote:false,
        auth_block_automatic:0, remote_unblock_remote:false, cmd_nonce:1,
        cmd_timestamp_ms:0, cmd_sig_ok:true, tcu_ack:true, cancel_request:false,
        auth_manual_out:false, password_attempt:2088290703, sim_time_ms:0
      };
      try{localStorage.removeItem("reb_inputs"); localStorage.removeItem("reb_cmd_pulse");}catch(ex){}
    }).catch(function(){});
  }
  useEffect(function(){return function(){stopTurbo();};},[]);

  /**
   * @brief Polls /api/snapshot every second to synchronise rebOut with the backend.
   *
   * Handles rising-edge detection for notify_theft and notify_blocked to trigger
   * smartphone flow transitions. Manages panel lockout state via rising-edge
   * detection on panel_locked_out. Updates sim_time_ms in currentInputsRef from
   * each snapshot. Clears the theft countdown interval when the component unmounts.
   */
  useEffect(function(){
    var active=true;
    function poll(){
      if(!active)return;
      fetch("/api/snapshot").then(function(r){return r.json();}).then(function(d){
        if(!active)return;
        if(d && typeof d === "object") setRebOut(d);

        if(d.notify_theft&&!prevNotifyTheftRef.current){
          prevNotifyTheftRef.current=true;
          var curFlow=phoneFlowRef.current;
          if(curFlow!=="blocking_pending"&&curFlow!=="unlocking_wait"){
            addLog(mkEntry("","Theft attempt detected on your vehicle. Confirm?","alert"));
            setPhoneFlow("theft_question");
            setTheftCd(THEFT_TIMEOUT_S);
            if(theftIntervalRef.current)clearInterval(theftIntervalRef.current);
            theftIntervalRef.current=setInterval(function(){
              setTheftCd(function(c){
                if(c<=1){clearInterval(theftIntervalRef.current);if(doTheftSimRef.current)doTheftSimRef.current();return THEFT_TIMEOUT_S;}
                return c - turboMultRef.current;
              });
            }, Math.max(16, Math.round(1000 / turboMultRef.current)));
          }
        } else if(!d.notify_theft){prevNotifyTheftRef.current=false;}

        if(phoneFlowRef.current==="unlocking_wait"&&d.state==="IDLE"){
          addLog(mkEntry("","Vehicle successfully unblocked","success"));
          setPhoneFlow("idle");
        }

        if(d.state==="IDLE" &&
          (phoneFlowRef.current==="blocked_confirm" ||
          phoneFlowRef.current==="blocking_pending")){
          setPhoneFlow("idle");
        }

        if(d.notify_blocked&&!prevNotifyBlockedRef.current){
          prevNotifyBlockedRef.current=true;
          addLog(mkEntry("","Vehicle successfully blocked. Current location: -8.0631, -34.8711","alert"));
          setPhoneFlow("blocked_confirm");
        } else if(!d.notify_blocked){prevNotifyBlockedRef.current=false;}

        setSfOn(!!(d.notify_theft||(d.sensor_score>=0.7)));
        setLkd(!!d.blocked_flag);

        var nowLocked = !!d.panel_locked_out;
        if(nowLocked && !prevPanelLockedRef.current){
          setLkUntil(Date.now()+LOCKOUT_SECONDS*1000);
          setLkCd(LOCKOUT_SECONDS);
        } else if(!nowLocked){
          setLkUntil(null);
          setLkCd(0);
          setAtt(0);
        }
        prevPanelLockedRef.current = nowLocked;

        currentInputsRef.current.sim_time_ms=d.sim_time_ms||0;
      }).catch(function(){});
    }
    poll();
    var tid=setInterval(poll,1000);
    return function(){
      active=false;clearInterval(tid);
      if(theftIntervalRef.current)clearInterval(theftIntervalRef.current);
    };
  },[]);

  /**
   * @brief Handles cross-tab synchronisation via localStorage storage events.
   *
   * Two channels are monitored:
   *  - "reb_cmd_pulse": ephemeral command from the Technical Interface; advances
   *    phoneFlow to reflect the command issued.
   *  - "reb_inputs": persistent input state written by the Technical Interface;
   *    merges into currentInputsRef and updates the ignition visual if changed.
   */
  useEffect(function(){
    function onStorage(e){
      if(e.key==="reb_cmd_pulse"&&e.newValue){
        try{
          var p=JSON.parse(e.newValue);
          if(p.auth_blocked_remote){
            if(phoneFlowRef.current==="idle"||phoneFlowRef.current==="unlocking_wait"){
              addLog(mkEntry("","Remote block triggered by Technical Interface","warn"));
              setPhoneFlow("blocking_pending");
            }
          }
          if(p.remote_unblock_remote){
            if(phoneFlowRef.current==="blocked_confirm"){
              setPhoneFlow("unlocking_wait");
            }
          }
          if(p.auth_manual_out){
            addLog(mkEntry("","Manual block triggered by Technical Interface","warn"));
            if(phoneFlowRef.current==="idle")setPhoneFlow("blocking_pending");
          }
          if(p.cancel_request){
            addLog(mkEntry("","Cancel request from Technical Interface","success"));
            setPhoneFlow("idle");
          }
          if(Number(p.auth_block_automatic)===1){
            addLog(mkEntry("","Auto-block triggered by Technical Interface","success"));
            setPhoneFlow("blocking_pending");
          }
        }catch(ex){}
        return;
      }
      if(e.key!=="reb_inputs"||!e.newValue)return;
      try{
        var q=JSON.parse(e.newValue);
        var VKEYS=["speed_sig_status","tcu_ack","ip_rx_ok","sms_rx_ok","cmd_sig_ok"];
        var vChanged=VKEYS.some(function(k){
          return q[k]!==undefined && q[k]!==currentInputsRef.current[k];
        });
        Object.keys(q).forEach(function(k){currentInputsRef.current[k]=q[k];});
        if(q.ignition_state!=null){
          var ni=Math.max(0,Math.min(3,Number(q.ignition_state)));
          setIgnIdx(ni);
        }
        if(vChanged){ apiStep({}); }
        setSyncTick(function(t){return t+1;});
      }catch(ex){}
    }
    window.addEventListener("storage",onStorage);
    return function(){window.removeEventListener("storage",onStorage);};
  },[]);

  function handleIgnNext(){
    var ni=Math.min(IGNITION_ORDER.length-1,ignIdx+1);
    setIgnIdx(ni);
    apiStep({ignition_state:ni});
  }
  function handleIgnPrev(){
    var ni=Math.max(0,ignIdx-1);
    setIgnIdx(ni);
    apiStep({ignition_state:ni});
  }

  /**
   * @brief Toggles a window between "normal" and "cracked" and updates sensor inputs.
   *
   * When cracked, glass_break_flag and accel_peak are both set to 3.0 and sent to
   * the backend. Restoring to normal resets both to 0.0.
   *
   * @param {string} which  "front" for the windshield, any other value for the side window.
   */
  function togWin(which){
    var cur=which==="front"?fWin:sWin,nxt=cur==="normal"?"cracked":"normal";
    (which==="front"?setFWin:setSWin)(nxt);
    if(nxt==="cracked"){
      setAPk(3);setGPk(3);setSfOn(true);
      setSLog(function(l){return [{msg:"["+ts()+"] "+(which==="front"?"WINDSHIELD":"SIDE WINDOW")+" CRACKED",alert:true}].concat(l);});
      apiStep({glass_break_flag:3.0,accel_peak:3.0});
    } else {
      setAPk(0);setGPk(0);setSfOn(false);
      setSLog(function(l){return [{msg:"["+ts()+"] Window restored",alert:false}].concat(l);});
      apiStep({glass_break_flag:0.0,accel_peak:0.0});
    }
  }

  /**
   * @brief Submits a panel password attempt to the backend.
   *
   * Hashes pw with hashPassword, then calls apiStep with the hash and either
   * auth_manual_out (block intent) or cancel_request (unblock intent).
   * On success advances p1Sub to the appropriate progress state.
   * On failure increments the attempt counter and arms the lockout timer if the
   * backend reports panel_locked_out.
   */
function submitPw(){
    if(rebOut.panel_locked_out || lkUntil) return;
    var hash = hashPassword(pw);
    setPw("");
    var step = {password_attempt: hash};
    step[p1Intent === "block" ? "auth_manual_out" : "cancel_request"] = true;
    apiStep(step, "MANUAL").then(function(d){
        if(!d) return;
        if(d.panel_password_ok){
          setPwSt("ok"); setAtt(0);
          setSLog(function(l){return [{msg:"["+ts()+"] Access granted",alert:false}].concat(l);});
          if(p1Intent === "block"){
            setP1Sub("blocking_progress");
          } else {
            setP1Sub(d.state === "IDLE" ? "unblocked_done" : "unblocking_progress");
          }
          setTimeout(function(){setPwSt("idle");}, 1000);
        }
         else {
            var n = d.panel_attempt_count || 0;
            setPwSt("wrong"); setAtt(n);
            setSLog(function(l){return [{msg:"["+ts()+"] Attempt "+n+"/"+MAX_ATTEMPTS+" — FAILED",alert:true}].concat(l);});
            if(d.panel_locked_out){
                setLkUntil(Date.now() + LOCKOUT_SECONDS*1000); setLkCd(LOCKOUT_SECONDS);
                setSLog(function(l){return [{msg:"["+ts()+"] PANEL LOCKED OUT "+LOCKOUT_SECONDS+"s",alert:true}].concat(l);});
                setPwSt("idle");
            } else {
                setTimeout(function(){setPwSt("idle");}, 1000);
            }
        }
    });
}

  /**
   * @brief Sends auth_manual_out=true to trigger a manual block via the panel.
   * @brief Sends cancel_request=true to cancel or reverse an active block.
   */
  function sendManualBlock(){
    addLog(mkEntry("","Manual block sent","warn"));
    setSLog(function(l){return [{msg:"["+ts()+"] AUTH_MANUAL_OUT (block)",alert:true}].concat(l);});
    apiStep({auth_manual_out:true},"MANUAL");
  }
  function sendManualUnblock(){
    addLog(mkEntry("","Manual unblock sent","success"));
    setSLog(function(l){return [{msg:"["+ts()+"] CANCEL_REQUEST (unblock)",alert:false}].concat(l);});
    apiStep({cancel_request:true},"MANUAL");
  }

  /**
   * @brief Ref holding the current doTheftSim callback.
   *
   * Reassigned on every render so that the theft countdown interval always
   * calls the latest closure without a stale reference to state.
   */
  doTheftSimRef.current=function(){
    if(theftIntervalRef.current)clearInterval(theftIntervalRef.current);
    setPhoneFlow("blocking_pending");
    addLog(mkEntry("","Timeout: auto-block confirmed","success"));
    apiStep({auth_block_automatic:1});
  };
  function doPhoneTheftSim(){
    if(theftIntervalRef.current)clearInterval(theftIntervalRef.current);
    setPhoneFlow("blocking_pending");
    addLog(mkEntry("","User confirmed: block vehicle","success"));
    apiStep({auth_block_automatic:1});
  }
  function doPhoneTheftNo(){
    if(theftIntervalRef.current)clearInterval(theftIntervalRef.current);
    setPhoneFlow("idle");
    addLog(mkEntry("","User dismissed: do not block","warn"));
    apiStep({auth_block_automatic:2});
  }
  function doPhoneLockSim(){
    addLog(mkEntry("","Vehicle blocking process initiated","warn"));
    setPhoneFlow("blocking_pending");
    apiStep({auth_blocked_remote:true});
  }
  function doPhoneLockNo(){}

  /**
   * @brief Initiates remote unlock; transitions to "unlocking_wait" and waits
   *        for the polling loop to confirm state === "IDLE" before completing.
   */
  function doPhoneUnlockSim(){
    setPhoneFlow("unlocking_wait");
    apiStep({remote_unblock_remote:true},"MANUAL");
  }
  function doPhoneUnlockNo(){}

  /** @brief POSTs to /api/test/nfr_rel_001 and logs the pass/fail result. */
  function handleNfrRel001(){
    fetch("/api/test/nfr_rel_001",{method:"POST"})
      .then(function(r){return r.json();}).then(function(d){
        var s=d.pass?"PASS":"FAIL";
        addLog(mkEntry("","Post Reset Recovery Test "+s,"info"));
        setSLog(function(l){return [{msg:"["+ts()+"] Post Reset Recovery Test "+s,alert:!d.pass}].concat(l);});
        }).catch(function(){});
  }

  var sysState=lkd?"BLOCKED":sfOn?"ALERT":ignState==="off"?"INACTIVE":"ACTIVE";
  var sysColor=lkd?"#f00":sfOn?"#f80":ignState==="off"?"#555":"#0f0";
  var micro={fontSize:"3.5px",fontFamily:"'Courier New',monospace"};
  var tiny ={fontSize:"4px",  fontFamily:"'Courier New',monospace"};

  /**
   * @brief Builds a pedal button element with hold-repeat press/release handlers.
   *
   * @param {object} cfg  { label, short, color, flex, ox, pressed, down, up }
   * @return React element representing a single pedal.
   */
  function mkPedal(cfg){
    return h("div",{key:cfg.label,style:{flex:cfg.flex,display:"flex",flexDirection:"column",alignItems:"center",gap:3,height:"100%",transform:"translateX("+cfg.ox+"px)"}},
      h("div",{style:{fontSize:"8px",fontFamily:"'Courier New',monospace",fontWeight:"bold",color:cfg.pressed?cfg.color:"#666",letterSpacing:1,textShadow:cfg.pressed?"0 0 6px "+cfg.color:"none",transition:"color .08s"}},cfg.label),
      h("button",{onMouseDown:cfg.down,onMouseUp:cfg.up,onMouseLeave:cfg.up,onTouchStart:cfg.down,onTouchEnd:cfg.up,
        style:{flex:1,width:"100%",background:cfg.pressed?"linear-gradient(180deg,"+cfg.color+"33,"+cfg.color+"18)":"linear-gradient(180deg,#1a1a1a,#0d0d0d)",
          border:"1.5px solid "+(cfg.pressed?cfg.color:"#2a2a2a"),borderRadius:"4px 4px 6px 6px",
          borderBottom:"4px solid "+(cfg.pressed?cfg.color:"#1a1a1a"),cursor:"pointer",
          transform:cfg.pressed?"translateY(2px) scaleY(.97)":"none",transition:"all .06s ease",position:"relative",overflow:"hidden"}},
        h("span",{style:{fontFamily:"'Courier New',monospace",fontSize:"16px",fontWeight:"bold",color:cfg.pressed?cfg.color:"#444",userSelect:"none"}},cfg.short)));
  }

  /**
   * @brief Renders infotainment page 1: block/unblock panel with PIN entry.
   *
   * Sub-states and their rendered content:
   *  - home_block:           prompt to block.
   *  - home_unblock:         vehicle-blocked status with UNBLOCK button.
   *  - blocking_progress:    in-progress indicator.
   *  - unblocking_progress:  in-progress indicator.
   *  - unblocked_done:       confirmation screen (auto-clears after 10 s).
   *  - pw_entry:             numeric keypad; shows lockout screen when locked out.
   */
  function infoPage1(){
    var ro=rebOut;
    var attCount=ro.panel_attempt_count||0;
    var isLocked=ro.panel_locked_out||!!lkUntil;

    var base={height:"100%",display:"flex",flexDirection:"column",
              alignItems:"center",justifyContent:"center",
              background:"#080808",padding:"3px",gap:2,textAlign:"center"};
    var btnBlk={padding:"1.5px 6px",fontSize:"3.5px",fontFamily:"'Courier New',monospace",
                border:"0.5px solid #f44",background:"#120000",color:"#f44",
                borderRadius:1,cursor:"pointer",fontWeight:"bold"};
    var btnUnblk={padding:"1.5px 6px",fontSize:"3.5px",fontFamily:"'Courier New',monospace",
                  border:"0.5px solid #0f0",background:"#001200",color:"#0f0",
                  borderRadius:1,cursor:"pointer",fontWeight:"bold"};

    if(p1Sub==="home_block"){
      return h("div",{style:base},
        h("span",{style:Object.assign({},tiny,{color:"#888",lineHeight:1.5,marginBottom:2})},
          "Block\nthe vehicle?"),
        h("button",{
          onClick:function(){setP1Intent("block");setPw("");setPwSt("idle");setP1Sub("pw_entry");},
          style:btnBlk},"BLOCK"));
    }

    if(p1Sub==="home_unblock"){
      return h("div",{style:base},
        h("div",{style:{width:8,height:8,borderRadius:"50%",background:"#f00",
                        boxShadow:"0 0 4px #f00",marginBottom:2}}),
        h("span",{style:Object.assign({},tiny,{color:"#f44",fontWeight:"bold",lineHeight:1.5})},
          "Vehicle is\nblocked"),
        h("button",{
          onClick:function(){setP1Intent("unblock");setPw("");setPwSt("idle");setP1Sub("pw_entry");},
          style:Object.assign({},btnUnblk,{marginTop:2})},"UNBLOCK"));
    }

    if(p1Sub==="blocking_progress"){
      return h("div",{style:base},
        h("div",{style:{fontSize:"8px",marginBottom:1}}),
        h("span",{style:Object.assign({},tiny,{color:"#f80",lineHeight:1.5})},
          "Vehicle is being\nblocked..."));
    }

    if(p1Sub==="unblocking_progress"){
      return h("div",{style:base},
        h("div",{style:{fontSize:"8px",marginBottom:1}}),
        h("span",{style:Object.assign({},tiny,{color:"#0af",lineHeight:1.5})},
          "Vehicle is being\nunblocked..."));
    }

    if(p1Sub==="unblocked_done"){
      return h("div",{style:base},
        h("div",{style:{width:8,height:8,borderRadius:"50%",background:"#0f0",
                        boxShadow:"0 0 4px #0f0",marginBottom:2}}),
        h("span",{style:Object.assign({},tiny,{color:"#0f0",fontWeight:"bold",lineHeight:1.5})},
          "Vehicle is\nunblocked"));
    }

    var numKeys=[1,2,3,4,5,6,7,8,9,null,0,"OK"];
    var pwDisplay=pwSt==="ok"?"OK":pwSt==="wrong"?"ERR":("\u25CF".repeat(pw.length)||"____");
    var dots=[];
    for(var i=0;i<MAX_ATTEMPTS;i++)
      dots.push(h("div",{key:i,style:{width:3,height:3,borderRadius:"50%",
        background:i<attCount?"#f00":"#1a1a1a",boxShadow:i<attCount?"0 0 2px #f00":"none"}}));

    var intentColor=p1Intent==="block"?"#f44":"#0f0";
    var intentLabel=p1Intent==="block"?"BLOCK":"UNBLOCK";

    return h("div",{style:{height:"100%",display:"flex",flexDirection:"column",
                            background:"#080808",padding:"1.5px",gap:0}},
      h("div",{style:{display:"flex",justifyContent:"space-between",alignItems:"center",
                      marginBottom:1,flexShrink:0}},
        h("span",{style:Object.assign({},micro,{color:intentColor,fontWeight:"bold"})},intentLabel),
        h("div",{style:{display:"flex",gap:1,alignItems:"center"}},
          dots,
          h("button",{
            onClick:function(){setP1Sub(p1Intent==="unblock"?"home_unblock":"home_block");setPw("");setPwSt("idle");},
            style:{background:"none",border:"none",color:"#333",cursor:"pointer",
                   fontSize:"4px",padding:0,marginLeft:2}},"←"))),
      isLocked
        ? h("div",{style:{flex:1,display:"flex",flexDirection:"column",alignItems:"center",
                           justifyContent:"center",gap:1}},
            h("span",{style:Object.assign({},tiny,{color:"#f00",fontWeight:"bold"})},"LOCKED OUT"),
            h("span",{style:{fontSize:"9px",color:"#f80",fontFamily:"monospace",fontWeight:"bold"}},lkCd,"s"),
            h("span",{style:Object.assign({},micro,{color:"#555"})},"Wait before retrying"))
        : h(Fragment,null,
            h("div",{style:{background:pwSt==="ok"?"#001200":pwSt==="wrong"?"#120000":"#040404",
                             border:"0.5px solid "+(pwSt==="ok"?"#0f0":pwSt==="wrong"?"#f00":"#1a1a1a"),
                             borderRadius:1,padding:"1px 2px",display:"flex",
                             justifyContent:"space-between",alignItems:"center",
                             marginBottom:1,flexShrink:0}},
              h("span",{style:Object.assign({},tiny,{color:pwSt==="ok"?"#0f0":pwSt==="wrong"?"#f00":"#0af",letterSpacing:1.5})},pwDisplay),
              h("button",{
                onClick:function(){setPw(function(p){return p.slice(0,-1);});},
                style:{background:"none",border:"none",color:"#333",cursor:"pointer",fontSize:"5px",padding:0}},
                "\u232B")),
            h("div",{style:{display:"grid",gridTemplateColumns:"repeat(3,1fr)",gap:1,flex:1}},
              numKeys.map(function(k,i){
                return h("button",{key:i,
                  onClick:function(){
                    if(k===null)return;
                    if(k==="OK"){submitPw();return;}
                    setPw(function(p){return p.length<10?p+String(k):p;});
                  },
                  style:{background:k==="OK"?"#001400":"#0c0c0c",
                         border:"0.5px solid "+(k==="OK"?"#0f0":"#1a1a1a"),
                         color:k==="OK"?"#0f0":k===null?"transparent":"#888",
                         borderRadius:1,fontSize:"4px",fontFamily:"'Courier New',monospace",
                         cursor:k===null?"default":"pointer",padding:"0"}},
                  k===null?"":String(k));})))
    );
  }

  /**
   * @brief Renders infotainment page 2: real-time signal status grid.
   *
   * Displays six LED indicators for: starter inhibit/ok, visual alert,
   * sonic alert, powertrain validity, TCU channel, and anti-replay status.
   */
  function infoPage2(){
    var ro=rebOut,ar=ro.anti_replay||{};
    var sigs=[
      {l:"STARTER",c:ro.starter_inhibit_active?"#f00":ro.starter_ok?"#0f0":"#555",v:ro.starter_inhibit_active?"INH":ro.starter_ok?"OK":"\u2014"},
      {l:"VIS",    c:ro.alert_visual?"#f80":"#222",   v:ro.alert_visual?"ON":"OFF"},
      {l:"SOUND",    c:ro.alert_sonic?"#f80":"#222",    v:ro.alert_sonic?"ON":"OFF"},
      {l:"PWT",    c:ro.powertrain_valid?"#0f0":"#f00",v:ro.powertrain_valid?"OK":"ERR"},
      {l:"TCU",    c:ro.channel_rx_ok?"#0f0":"#f00",  v:ro.channel_rx_ok?"OK":"FAIL"},
      {l:"ANTI-R", c:ar.ok?"#0f0":"#f44",             v:ar.ok?"OK":"REJ"},
    ];
    return h("div",{style:{display:"grid",gridTemplateColumns:"1fr 1fr 1fr",gridTemplateRows:"1fr 1fr",height:"100%",gap:".5px",background:"#111",padding:"1.5px"}},
      sigs.map(function(s,i){return h("div",{key:i,style:{background:"#080808",borderRadius:2,display:"flex",flexDirection:"column",alignItems:"center",justifyContent:"center",gap:1}},
        h("div",{style:{width:6,height:6,borderRadius:"50%",background:s.c,boxShadow:"0 0 4px "+s.c}}),
        h("span",{style:Object.assign({},micro,{color:s.c,fontWeight:"bold"})},s.v),
        h("span",{style:Object.assign({},micro,{color:"#333"})},s.l));}));
  }

  /**
   * @brief Renders infotainment page 3: diagnostic console with NFR export.
   *
   * Displays a two-column key-value grid of critical runtime values and the
   * most recent event log entry. Provides two action buttons:
   *  - NFR-REL-001 test trigger.
   *  - NFR-INFO-001 diagnostic log export (ASC-style TXT download).
   *
   * The export function encodes the current rebOut snapshot and event log
   * into synthetic CAN frames using the DBC message layout from can_defs.h.
   * Event log entries are packed into 8-byte REB_LOG_FRAME (CAN ID 0x0403)
   * records with the layout: [seq|type][evt][from][to][src][auth_fail][ts_hi][ts_lo].
   */
  function infoPage3(){
    var ro=rebOut,ar=ro.anti_replay||{};
    var lastLog=ro.logs&&ro.logs.length>0?ro.logs[ro.logs.length-1]:null;
    function fmtLog(lg){if(!lg)return "-";return (EVT_NAMES_MAP[lg.kind]||"EVT"+lg.kind)+" @"+lg.ts_ms+"ms";}
    var rows=[
      ["STATE",  ro.state||"IDLE"],
      ["DERATE", (ro.derate_pct||100)+"%"],
      ["SCORE",  (ro.sensor_score||0).toFixed(2)],
      ["PARKED", (ro.parked_timer_s||0)+"s"],
      ["REV",    (ro.reversal_timer_s||0)+"s"],
      ["IP",     currentInputsRef.current.ip_rx_ok  ? "ON" : "OFF"],
      ["SMS",    currentInputsRef.current.sms_rx_ok ? "ON" : "OFF"],
      ["TCU ACK",currentInputsRef.current.tcu_ack   ? "ON" : "OFF"],
      ["NONCE",  ar.last_nonce!=null?ar.last_nonce:"\u2014"],
      ["TS",     ro.sim_time_ms||0],
    ];
    function handleNfrInfo001Export(){
      var DBC_MSG = {
        0x100:{ name:"TCU_STATUS",       dir:"Rx", dlc:1, node:"TCU",       period:"50ms"  },
        0x103:{ name:"TCU_AUTH",         dir:"Rx", dlc:1, node:"TCU",       period:"evt"   },
        0x200:{ name:"REB_CMD",          dir:"Rx", dlc:8, node:"TCU",       period:"evt"   },
        0x201:{ name:"REB_STATUS",       dir:"Tx", dlc:1, node:"REB",       period:"100ms" },
        0x202:{ name:"TCU_ACK",          dir:"Rx", dlc:1, node:"TCU",       period:"evt"   },
        0x300:{ name:"TCU_TO_REB",       dir:"Rx", dlc:8, node:"TCU",       period:"evt"   },
        0x105:{ name:"ECU_POWERTRAIN",   dir:"Rx", dlc:5, node:"ECU_FUEL",  period:"100ms" },
        0x110:{ name:"ECU_SENSOR_BCM",   dir:"Rx", dlc:5, node:"BCM",       period:"20ms"  },
        0x120:{ name:"ECU_PANEL",        dir:"Rx", dlc:1, node:"PANEL",     period:"50ms"  },
        0x121:{ name:"ECU_PANEL_AUTH",   dir:"Rx", dlc:4, node:"PANEL",     period:"evt"   },
        0x400:{ name:"REB_DERATE_CMD",   dir:"Tx", dlc:2, node:"REB",       period:"10ms"  },
        0x401:{ name:"REB_TO_BCM",       dir:"Tx", dlc:1, node:"REB",       period:"evt"   },
        0x500:{ name:"VEHICLE_STATE",    dir:"Rx", dlc:8, node:"VEHICLE",   period:"100ms" },
        0x501:{ name:"BCM_INTRUSION",    dir:"Rx", dlc:8, node:"BCM",       period:"100ms" },
        0x502:{ name:"PANEL_AUTH_CMD",   dir:"Rx", dlc:8, node:"PANEL",     period:"evt"   },
        0x503:{ name:"PANEL_CANCEL_CMD", dir:"Rx", dlc:8, node:"PANEL",     period:"evt"   }
      };
      var EVT_LABEL = {
        0x01:"STATE_TRANSITION", 0x02:"AUTH_FAIL",       0x03:"AUTH_OK",
        0x04:"PANEL_LOCKOUT",    0x05:"SENSOR_THEFT",    0x06:"DERATE_ACTIVE",
        0x07:"STARTER_INHIBIT",  0x08:"UNBLOCK",         0x09:"REVERSAL_ABORT",
        0x0A:"REVERSAL_EXPIRE",  0x0B:"NVM_WRITE",       0x0C:"NVM_RESTORE",
        0x0D:"SIGNAL_FAULT",     0x0E:"CMD_RECEIVED",    0x0F:"SPEED_SAFE_STOP",
        0x10:"BLOCK_REJ_SIGNAL", 0x11:"BLOCK_REJ_SPEED", 0x12:"RX_SUPV_FAIL"
      };
      var STATE_ID = {0:"IDLE",1:"THEFT_CONFIRMED",2:"BLOCKING",3:"BLOCKED"};
      var SRC_ID   = {0:"SOURCE_PANEL",1:"SOURCE_REMOTE",2:"SOURCE_AUTO"};
      var h2=function(n){return("0"+n.toString(16).toUpperCase()).slice(-2);};
      var h4=function(n){return("000"+n.toString(16).toUpperCase()).slice(-4);};
      var ts6=function(ms){return(ms/1000).toFixed(6).padStart(14," ");};
      var col=function(v,w){return String(v).padEnd(w," ");};
      var ar=ro.anti_replay||{};
      var simMs=ro.sim_time_ms||0;
      var b201=(ro.notify_theft?1:0)|((ro.blocked_flag?1:0)<<1);
      var b400_0=Math.max(0,Math.min(100,ro.derate_pct||100));
      var b400_1=(ro.starter_ok?1:0);
      var b401=(ro.alert_visual?1:0)|((ro.alert_sonic?1:0)<<1);
      var spd_raw=Math.round((ro.vehicle_speed_kmh||0)*100);
      var rpm_raw=Math.round((ro.rpm||0)*4);
      var bcm_peak_raw=Math.round((ro.sensor_score||0)*100*100);
      var L=[];
      var exportTime=new Date().toISOString();
      L.push("date "+exportTime);
            
      L.push("// REB Diagnostic Log ");
      L.push("// SIM_TIME: "+simMs+" ms  |  Export: "+exportTime);
      L.push("//");
      L.push("// "+col("Timestamp",14)+" "+col("Ch",2)+" "+col("ID  ",6)+" "+
             col("DLC",3)+" "+col("Data (Hex Payload)",24)+" "+
             col("Signal Name",22)+"  Decoded Value");
      L.push("// "+Array(110).join("-"));
      var frames=[
        { ts:simMs, id:0x201, data:[b201] },
        { ts:simMs, id:0x400, data:[b400_0, b400_1] },
        { ts:simMs, id:0x401, data:[b401] },
        { ts:simMs, id:0x105, data:[ 0x02,
            (rpm_raw>>8)&0xFF, rpm_raw&0xFF,
            (spd_raw>>8)&0xFF, spd_raw&0xFF] },
        { ts:simMs, id:0x110, data:[
            (bcm_peak_raw>>8)&0xFF, bcm_peak_raw&0xFF, 0,0,0] }
      ];
      frames.forEach(function(f){
        var desc=DBC_MSG[f.id];
        if(!desc) return;
        var hexBytes=f.data.map(h2).join(" ");
        var decoded=decodeFrame(f.id,f.data,STATE_ID,SRC_ID,ro);
        L.push(
          col(ts6(f.ts),14)+" "+
          col("1",2)+" "+
          col(h4(f.id),6)+" "+
          col("d "+desc.dlc,3)+" "+
          col(hexBytes,24)+" "+
          col(desc.name,22)+" "+
          " // "+decoded
        );
      });
      var STATE_NUM={"IDLE":0,"THEFT_CONFIRMED":1,"BLOCKING":2,"BLOCKED":3};
      var logs=ro.logs||[];
      if(logs.length>0){
        L.push("//");
        L.push("// --- EVENT LOG (NFR-INFO-001) — CAN ID 0x0403 REB_LOG_FRAME ---");
        L.push("// Layout (8B): [seq_id|frame_type] [evt_code] [state_from] [state_to] [source] [auth_fail] [ts_high] [ts_low]");
        logs.forEach(function(e,idx){
          var evtCode=e.kind||0;
          var evtLabel=EVT_LABEL[evtCode]||("EVT_0x"+h2(evtCode));
          var sfStr=e.state_from||"--";
          var stStr=e.state_to  ||"--";
          var sfNum=STATE_NUM[sfStr]!=null?STATE_NUM[sfStr]:0xFF;
          var stNum=STATE_NUM[stStr]!=null?STATE_NUM[stStr]:0xFF;
          var srcNum=e.source!=null?e.source:0xFF;
          var afNum =e.auth_fail||0;
          var tsMs  =e.ts_ms||0;
          var tsLow =tsMs&0xFF;
          var tsHigh=(tsMs>>8)&0xFF;
          var seqB  =(idx&0xF0)|0x01;
          var payload=[seqB,evtCode,sfNum,stNum,srcNum,afNum,tsHigh,tsLow];
          var hexBytes=payload.map(h2).join(" ");
          var srcLabel=SRC_ID[srcNum]||("SRC_0x"+h2(srcNum));
          var decoded="EVT="+evtLabel+
            "  FROM="+sfStr+" (0x"+h2(sfNum)+")"+
            " TO="+stStr+" (0x"+h2(stNum)+")"+
            "  SRC="+srcLabel+
            (afNum?"  AUTH_FAIL=0x"+h2(afNum):"");
          L.push(
            col(ts6(tsMs),14)+" "+
            col("1",2)+" "+
            col("0403",6)+" "+
            col("d 8",3)+" "+
            col(hexBytes,24)+" "+
            col("REB_LOG_FRAME",22)+
            " // "+decoded
          );
        });
      }
      L.push("//");
      L.push("// End of REB Diagnostic Log");
      var blob=new Blob([L.join("\r\n")],{type:"text/plain"});
      var url=URL.createObjectURL(blob);
      var a=document.createElement("a");
      a.href=url;
      a.download="REB_DiagnosticLog_"+Date.now()+".txt";
      document.body.appendChild(a);a.click();
      document.body.removeChild(a);URL.revokeObjectURL(url);
    }

    /**
     * @brief Decodes a single CAN frame payload into a human-readable string.
     *
     * Signal layouts follow the DBC definitions in can_defs.h. Returns a
     * space-separated hex dump for unrecognised IDs.
     *
     * @param {number} id       CAN message ID.
     * @param {Array}  data     Byte array (payload).
     * @param {object} STATE_ID Numeric → name map for reb_state_t.
     * @param {object} SRC_ID   Numeric → name map for activation_source_t.
     * @param {object} ro       Current rebOut snapshot (for sensor_score).
     * @return {string}         Decoded signal description string.
     */
    function decodeFrame(id,data,STATE_ID,SRC_ID,ro){
      var h2=function(n){return("0"+n.toString(16).toUpperCase()).slice(-2);};
      if(id===0x201){
        var nt=data[0]&1, nb=(data[0]>>1)&1;
        return "notify_theft="+nt+" ("+( nt?"THEFT_DETECTED":"NO_THEFT")+
               ")  notify_blocked="+nb+" ("+( nb?"VEHICLE_BLOCKED":"NOT_BLOCKED")+")";
      }
      if(id===0x400){
        var dp=data[0]||0, sk=data[1]&1;
        return "derate_pct="+dp+"% (0x"+h2(dp)+")  starter_ok="+sk+
               " ("+(sk?"ALLOW_START":"BLOCK_START")+")";
      }
      if(id===0x401){
        var av=data[0]&1, as_=(data[0]>>1)&1;
        return "alert_visual="+av+" ("+( av?"ALERT_ON":"ALERT_OFF")+
               ")  alert_sonic="+as_+" ("+( as_?"SONIC_ON":"MUTE")+")";
      }
      if(id===0x105){
        var ign=data[0]&0x03;
        var ign_names={0:"IGN_OFF",1:"IGN_ACC",2:"IGN_ON",3:"IGN_START"};
        var rpmRaw=((data[1]<<8)|data[2]);
        var spdRaw=((data[3]<<8)|data[4]);
        return "ign="+ign+" ("+ign_names[ign]+
               ")  rpm="+(rpmRaw*0.25).toFixed(0)+" rpm"+
               "  speed="+(spdRaw*0.01).toFixed(1)+" km/h";
      }
      if(id===0x110){
        var accelRaw=((data[0]<<8)|data[1]);
        var glassRaw=((data[2]<<8)|data[3]);
        return "accel_peak="+(accelRaw/100).toFixed(2)+
               "  glass_break_flag="+(glassRaw/100).toFixed(2)+
               "  sensor_score="+(ro.sensor_score||0).toFixed(4);
      }
      return data.map(h2).join(" ");
    }
    return h("div",{style:{display:"flex",flexDirection:"column",padding:"1.5px",background:"#080808",overflow:"hidden"}},
      h("div",{style:Object.assign({},tiny,{color:"#0af",fontWeight:"bold",padding:"1px 2px",borderBottom:"0.5px solid rgba(0,170,255,.2)",marginBottom:1})},"Diagnostic Console"),
      h("div",{style:{display:"grid",gridTemplateColumns:"1fr 1fr",gap:"0.5px",overflow:"hidden"}},
        rows.map(function(r,i){return h("div",{key:i,style:{display:"flex",justifyContent:"space-between",padding:"0.5px 2px",background:"#040404",borderRadius:1}},
          h("span",{style:Object.assign({},micro,{color:"#444"})},r[0]),
          h("span",{style:Object.assign({},micro,{color:"#0af"})},String(r[1])));})),
      lastLog&&h("div",{style:Object.assign({},micro,{color:"#f80",padding:"0.5px 2px",background:"#040404",borderRadius:1,marginTop:1,overflow:"hidden",whiteSpace:"nowrap"})},"EVT: "+fmtLog(lastLog)),
      h("button",{onClick:handleNfrRel001,style:{marginTop:1,padding:"1px 2px",fontSize:"3px",fontFamily:"'Courier New',monospace",border:"0.5px solid #0af",background:"#000d18",color:"#0af",borderRadius:1,cursor:"pointer",textAlign:"left"}}," Post-reset recovery Test"),
      h("div",{style:{marginTop:2,borderTop:"0.5px solid rgba(0,170,255,.15)",overflow:"hidden"}},
        h("button",{onClick:handleNfrInfo001Export,style:{padding:"1px 2px",fontSize:"3px",fontFamily:"'Courier New',monospace",border:"0.5px solid #08f",background:"#000a14",color:"#08f",borderRadius:1,cursor:"pointer",textAlign:"left",width:"100%"}},"Export TXT Log")
      )
    );
  }

  /** @brief Precomputed crack-line offsets relative to the impact point. */
  var crackLines=[[-10,-18],[-22,-8],[-18,8],[-6,22],[8,18],[19,10],[23,-3],[14,-18],[0,-24]];

  /**
   * @brief Fixed-position toolbar rendered outside the scaled scene viewport.
   *
   * Contains turbo multiplier selector buttons, a single-step button, a
   * run/stop toggle, and a reset button. The current simulation time is
   * displayed while the turbo loop is active.
   */
var turboBar=h("div",{style:{
        position:"fixed",bottom:0,left:0,right:0,height:22,
        background:"#080808",borderTop:"1px solid #1a1a1a",
        display:"flex",alignItems:"center",gap:4,padding:"0 10px",
        fontFamily:"'Courier New',monospace",zIndex:99999,pointerEvents:"auto"}},
      h("span",{style:{fontSize:9,color:"#444",marginRight:2}},"TURBO"),
      [1,10,100,1000].map(function(m){
        return h("button",{key:m,
          onClick:function(){setTurboMult(m);turboMultRef.current=m;},
          style:{padding:"1px 5px",fontSize:8,fontFamily:"inherit",cursor:"pointer",
            background:turboMult===m?"#001a08":"#0a0a0a",
            border:"1px solid "+(turboMult===m?"#0f0":"#222"),
            color:turboMult===m?"#0f0":"#444",borderRadius:2}},
          "x"+m);
      }),
      h("button",{
        onClick:function(){apiStep(null,"AUTO",turboMultRef.current);},
        disabled:turboRun,
        style:{padding:"1px 8px",fontSize:8,fontFamily:"inherit",cursor:"pointer",
          background:"#000d18",border:"1px solid #0af",
          color:turboRun?"#333":"#0af",borderRadius:2,marginLeft:6}},
        "▶ STEP"),
      h("button",{onClick:turboRun?stopTurbo:startTurbo,
        style:{padding:"1px 8px",fontSize:8,fontFamily:"inherit",cursor:"pointer",
          background:turboRun?"#1a0000":"#000d18",
          border:"1px solid "+(turboRun?"#f44":"#0af"),
          color:turboRun?"#f44":"#0af",borderRadius:2,marginLeft:2}},
        turboRun?"\u25A0 STOP":"\u25BA RUN"),
      h("button",{
        onClick:doReset,
        style:{padding:"1px 8px",fontSize:8,fontFamily:"inherit",cursor:"pointer",
          background:"#110000",border:"1px solid #622",
          color:"#c44",borderRadius:2,marginLeft:6}},
        "\u21BA RESET"),
      turboRun&&h("span",{style:{fontSize:8,color:"#0af",marginLeft:4,animation:"none"}},
        "sim running x"+turboMult+" \u2022 "+((rebOut.sim_time_ms||0)/1000).toFixed(1)+"s")
    );

  return h(Fragment,null,
   h("div",{style:{width:"100vw",height:"calc(100vh - 37px)",overflow:"hidden",background:"#000",position:"relative"}},
    h("div",{style:{position:"absolute",left:offX,top:offY,width:SCENE_W,height:SCENE_H,transformOrigin:"top left",transform:"scale("+scale+")",zIndex:0}},

      h("div",{style:{position:"absolute",left:0,top:-480,width:1280,height:800,overflow:"hidden",zIndex:-9999}},
        h(WindshieldView,{speed:speed})),

      h("div",{onClick:function(e){var rect=e.currentTarget.getBoundingClientRect();setCrack({x:(e.clientX-rect.left)*(1000/rect.width),y:(e.clientY-rect.top)*(200/rect.height)});togWin("front");},
        style:{position:"absolute",left:0,top:0,width:1000,height:200,zIndex:10,cursor:"pointer"}},
        fWin==="cracked"&&h("svg",{style:{position:"absolute",inset:0,pointerEvents:"none"},width:1000,height:200,viewBox:"0 0 1000 200"},
          h("defs",null,h("filter",{id:"cg"},h("feDropShadow",{dx:"0",dy:"0",stdDeviation:"3",floodColor:"#aaddff",floodOpacity:"0.7"}))),
          h("circle",{cx:crack.x,cy:crack.y,r:"4",fill:"rgba(180,220,255,.2)",stroke:"rgba(220,240,255,.75)",strokeWidth:"1",filter:"url(#cg)"}),
          crackLines.map(function(dl,i){return h("line",{key:i,x1:crack.x,y1:crack.y,x2:crack.x+dl[0],y2:crack.y+dl[1],stroke:"rgba(200,230,255,.7)",strokeWidth:"1.1",strokeLinecap:"round",filter:"url(#cg)"});}),
          h("ellipse",{cx:crack.x,cy:crack.y,rx:"13",ry:"10",fill:"none",stroke:"rgba(200,230,255,.12)",strokeWidth:".5",strokeDasharray:"3,5"}))),

      h("img",{src:"/static/images/quarto.png",alt:"interior",style:{position:"absolute",top:0,left:0,right:0,bottom:0,width:"100%",height:"100%",objectFit:"fill",display:"block"}}),

      h("div",{style:{position:"absolute",left:289,top:255,width:144,height:48,display:"flex",gap:"1%",pointerEvents:"auto"}},
        h("div",{onClick:function(){setZoom("speed");},style:{flex:1,height:"100%",cursor:"zoom-in"}},h(Gauge,{id:"sp",value:speed,maxValue:260,unit:"km/h",ticks:{step:20,major:60},labels:[0,60,120,180,240],digitalValue:String(speed).padStart(3,"0")})),
        h("div",{onClick:function(){setZoom("rpm");},style:{flex:1,height:"100%",cursor:"zoom-in"}},h(Gauge,{id:"rp",value:rpm,maxValue:8000,unit:"RPM x1000",ticks:{step:500,major:1000},labels:[0,2,4,6,8],redZone:[6500,8000],digitalValue:(rpm/1000).toFixed(1)}))),

      h("div",{style:{position:"absolute",left:276,top:504,width:203,height:85,display:"flex",gap:8,padding:"6px 10px",pointerEvents:"auto",alignItems:"flex-end"}},
        mkPedal({label:"ACCEL",short:"A",color:"#00cc44",flex:1.2,ox:-10,pressed:apOn,down:hAD,up:hAU}),
        mkPedal({label:"BRAKE",short:"F",color:"#ff3322",flex:1,ox:0,pressed:bpOn,down:hBD,up:hBU})),

      h("div",{onDoubleClick:function(){setZoom("phone");},style:{position:"absolute",left:830,top:242,pointerEvents:"auto"}},
        h(SmartphonePanel,{phoneLog:pLog,phoneFlow:phoneFlow,theftCd:theftCd,
          onTheftSim:doPhoneTheftSim,onTheftNo:doPhoneTheftNo,
          onLockSim:doPhoneLockSim,onLockNo:doPhoneLockNo,
          onUnlockSim:doPhoneUnlockSim,onUnlockNo:doPhoneUnlockNo})),

      h("div",{onClick:function(){setZoom("ignition");},style:{position:"absolute",left:495,top:358,width:42,height:42,pointerEvents:"auto"}},
        h(IgnitionKey,{state:ignState,onNext:handleIgnNext,onPrev:handleIgnPrev})),

      h("div",{onClick:function(){setZoom("info");},style:{position:"absolute",left:565.5,top:353.5,width:147,height:96,background:"#050505",border:"1px solid #1a1a1a",borderRadius:"3px",boxShadow:"0 0 8px rgba(0,160,255,.2),inset 0 0 6px #000",display:"flex",flexDirection:"column",overflow:"hidden",fontFamily:"'Courier New',monospace",pointerEvents:"auto"}},
        h("div",{style:{background:lkd?"#1a0000":sfOn?"#150800":"#000d18",borderBottom:"1px solid "+sysColor+"44",padding:"0.5px 3px",display:"flex",alignItems:"center",gap:3,flexShrink:0,height:"14%"}},
          h("span",{style:Object.assign({},tiny,{color:sysColor,fontWeight:"bold"})},sysState),
          h("div",{style:{display:"flex",gap:2,flex:1}},
            [{l:"GPS",ok:true},{l:"FNT",ok:fWin==="normal"},{l:"SIDE",ok:sWin==="normal"},{l:"VEH",ok:!lkd}].map(function(x){return h("div",{key:x.l,style:{display:"flex",alignItems:"center",gap:1}},h("div",{style:{width:3,height:3,borderRadius:"50%",background:x.ok?"#0f0":"#f00",boxShadow:"0 0 2px "+(x.ok?"#0f0":"#f00")}}),h("span",{style:Object.assign({},micro,{color:x.ok?"#080":"#f44"})},x.l));})),
          h("span",{style:Object.assign({},micro,{color:aPk>0?"#f80":"#222"})},"AP="+aPk.toFixed(1)),
          h("span",{style:Object.assign({},micro,{color:gPk>0?"#f80":"#222"})},"GP="+gPk.toFixed(1))),
        h("div",{style:{display:"flex",background:"#040404",flexShrink:0,height:"10%"}},
          ["AUTH","SIG","DIAG"].map(function(lbl,i){var p=i+1;return h("button",{key:p,onClick:function(e){e.stopPropagation();setPage(p);},style:{flex:1,background:page===p?"#001020":"transparent",color:page===p?"#0af":"#333",border:"none",borderBottom:page===p?"1px solid #0af":"1px solid transparent",cursor:"pointer",fontSize:"4px",fontFamily:"'Courier New',monospace",padding:"0 1px"}},lbl);})),
        h("div",{style:{flex:1,overflow:"hidden"}},page===1?infoPage1():page===2?infoPage2():infoPage3()),
        h("div",{style:{background:"#030303",borderTop:"0.5px solid #111",padding:"0.5px 4px",display:"flex",justifyContent:"space-between",alignItems:"center",height:"10%",flexShrink:0}},
          h("button",{onClick:function(e){e.stopPropagation();setPage(function(p){return Math.max(1,p-1);});},style:{background:"none",border:"none",color:page>1?"#0af":"#1a1a1a",cursor:"pointer",fontSize:"6px",padding:0}},"\u25C4"),
          h("div",{style:{display:"flex",gap:2}},[1,2,3].map(function(p){return h("div",{key:p,style:{width:3,height:3,borderRadius:"50%",background:page===p?"#0af":"#111"}});})),
          h("button",{onClick:function(e){e.stopPropagation();setPage(function(p){return Math.min(3,p+1);});},style:{background:"none",border:"none",color:page<3?"#0af":"#1a1a1a",cursor:"pointer",fontSize:"6px",padding:0}},"\u25BA"))),

      zoom&&h("div",{style:{position:"absolute",inset:0,backgroundColor:"rgba(0,0,0,.85)",zIndex:9999,display:"flex",justifyContent:"center",alignItems:"center",backdropFilter:"blur(4px)",pointerEvents:"auto"},onClick:function(){setZoom(null);}},
        h("div",{style:{position:"relative",width:zoom==="phone"?"400px":"600px",height:zoom==="phone"?"580px":"600px",transform:"scale(.85)"},onClick:function(e){e.stopPropagation();}},
          h("button",{onClick:function(){setZoom(null);},style:{position:"absolute",top:-30,right:-30,background:"#f00",color:"#fff",border:"none",borderRadius:"50%",width:30,height:30,cursor:"pointer",fontWeight:"bold",zIndex:10000}},"X"),
          zoom==="speed"&&h(Gauge,{id:"spz",value:speed,maxValue:260,unit:"km/h",ticks:{step:20,major:60},labels:[0,60,120,180,240],digitalValue:String(speed).padStart(3,"0")}),
          zoom==="rpm"&&h(Gauge,{id:"rpz",value:rpm,maxValue:8000,unit:"RPM x1000",ticks:{step:500,major:1000},labels:[0,2,4,6,8],redZone:[6500,8000],digitalValue:(rpm/1000).toFixed(1)}),
          zoom==="phone"&&h("div",{style:{transform:"scale(1.5)",transformOrigin:"top left"}},
            h(SmartphonePanel,{phoneLog:pLog,phoneFlow:phoneFlow,theftCd:theftCd,
              onTheftSim:doPhoneTheftSim,onTheftNo:doPhoneTheftNo,
              onLockSim:doPhoneLockSim,onLockNo:doPhoneLockNo,
              onUnlockSim:doPhoneUnlockSim,onUnlockNo:doPhoneUnlockNo})),
          zoom==="ignition"&&h(IgnitionKey,{state:ignState,onNext:handleIgnNext,onPrev:handleIgnPrev}),
          zoom==="info"&&h("div",{style:{width:600,height:600,background:"#050505",border:"1px solid #1a1a1a",borderRadius:"3px",display:"flex",flexDirection:"column",overflow:"hidden",fontFamily:"'Courier New',monospace"}},
            h("div",{style:{background:lkd?"#1a0000":sfOn?"#150800":"#000d18",borderBottom:"1px solid "+sysColor+"44",padding:"4px 8px",display:"flex",alignItems:"center",gap:8,flexShrink:0}},
              h("span",{style:{fontSize:"12px",color:sysColor,fontFamily:"'Courier New',monospace",fontWeight:"bold"}},"REB — "+sysState)),
            h("div",{style:{display:"flex",background:"#040404",flexShrink:0}},
              ["AUTH","SIG","DIAG"].map(function(lbl,i){var p=i+1;return h("button",{key:p,onClick:function(){setPage(p);},style:{flex:1,padding:"6px 0",background:page===p?"#001020":"transparent",color:page===p?"#0af":"#333",border:"none",borderBottom:page===p?"2px solid #0af":"2px solid transparent",cursor:"pointer",fontSize:"12px",fontFamily:"'Courier New',monospace"}},lbl);})),
            h("div",{style:{flex:1,overflow:"hidden",position:"relative"}},
              h("div",{style:{position:"absolute",top:0,left:0,width:147,height:66,
                transform:"scale(4)",transformOrigin:"top left"}},
                page===1?infoPage1():page===2?infoPage2():infoPage3())),
            h("div",{style:{background:"#030303",borderTop:"0.5px solid #111",padding:"4px 8px",display:"flex",justifyContent:"space-between",alignItems:"center",flexShrink:0}},
              h("button",{onClick:function(){setPage(function(p){return Math.max(1,p-1);});},style:{background:"none",border:"none",color:page>1?"#0af":"#1a1a1a",cursor:"pointer",fontSize:"16px",padding:0}},"\u25C4"),
              h("div",{style:{display:"flex",gap:6}},[1,2,3].map(function(p){return h("div",{key:p,style:{width:8,height:8,borderRadius:"50%",background:page===p?"#0af":"#111"}});})),
              h("button",{onClick:function(){setPage(function(p){return Math.min(3,p+1);});},style:{background:"none",border:"none",color:page<3?"#0af":"#1a1a1a",cursor:"pointer",fontSize:"16px",padding:0}},"\u25BA")))
        ))
    )),
    turboBar
  );
}

ReactDOM.createRoot(document.getElementById("root")).render(h(ControlInterface,null));
