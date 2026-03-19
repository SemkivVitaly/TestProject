/* Auto-generated from data/index.html - do not edit */
#ifndef BRIDGE_UI_EMBED_H
#define BRIDGE_UI_EMBED_H

#ifndef PROGMEM
#define PROGMEM
#endif

static const char BRIDGE_UI_HTML[] PROGMEM = R"BRIDGE_UI_RAW(<!DOCTYPE html>
<html lang="ru" xml:lang="ru" xmlns="http://www.w3.org/1999/xhtml" xmlns="http://www.w3.org/1999/html">
<head>
    <meta name="description" content="Настройки моста Bridge для ESP32">
    <meta name="author" content="Wolfgang Christl">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Мост Bridge для ESP32 — Настройки</title>
	<link rel="apple-touch-icon" sizes="180x180" href="apple-touch-icon.png">
	<link rel="icon" type="image/png" sizes="32x32" href="favicon-32x32.png">
	<link rel="icon" type="image/png" sizes="16x16" href="favicon-16x16.png">
    <meta name="theme-color" content="#3166FF">
    <style>.column,.columns,.container{width:100%;box-sizing:border-box}.container{position:relative;max-width:800px;margin:0 auto;padding:0 20px}.column,.columns{float:left}@media (min-width:400px){.container{width:85%;padding:0}}@media (min-width:550px){.container{width:80%}.column,.columns{margin-left:4%}.column:first-child,.columns:first-child{margin-left:0}.one.column,.one.columns{width:4.66666666667%}.two.columns{width:13.3333333333%}.three.columns{width:22%}.four.columns{width:30.6666666667%}.five.columns{width:39.3333333333%}.six.columns{width:48%}.seven.columns{width:56.6666666667%}.eight.columns{width:65.3333333333%}.nine.columns{width:74%}.ten.columns{width:82.6666666667%}.eleven.columns{width:91.3333333333%}.twelve.columns{width:100%;margin-left:0}.one-third.column{width:30.6666666667%}.two-thirds.column{width:65.3333333333%}.one-half.column{width:48%}.offset-by-one.column,.offset-by-one.columns{margin-left:8.66666666667%}.offset-by-two.column,.offset-by-two.columns{margin-left:17.3333333333%}.offset-by-three.column,.offset-by-three.columns{margin-left:26%}.offset-by-four.column,.offset-by-four.columns{margin-left:34.6666666667%}.offset-by-five.column,.offset-by-five.columns{margin-left:43.3333333333%}.offset-by-six.column,.offset-by-six.columns{margin-left:52%}.offset-by-seven.column,.offset-by-seven.columns{margin-left:60.6666666667%}.offset-by-eight.column,.offset-by-eight.columns{margin-left:69.3333333333%}.offset-by-nine.column,.offset-by-nine.columns{margin-left:78%}.offset-by-ten.column,.offset-by-ten.columns{margin-left:86.6666666667%}.offset-by-eleven.column,.offset-by-eleven.columns{margin-left:95.3333333333%}.offset-by-one-third.column,.offset-by-one-third.columns{margin-left:34.6666666667%}.offset-by-two-thirds.column,.offset-by-two-thirds.columns{margin-left:69.3333333333%}.offset-by-one-half.column,.offset-by-one-half.columns{margin-left:52%}}html{font-size:62.5%}body{font-size:1.5em;font-weight:400;font-family:"Raleway","HelveticaNeue","Helvetica Neue",Helvetica,Arial,sans-serif;color:#222}h1,h2,h3,h4,h5,h6{margin-top:0;margin-bottom:.8rem;font-weight:300}h1,h2,h3{font-size:4rem;line-height:1.2;letter-spacing:-.1rem}h2,h3{font-size:3.6rem;line-height:1.25}h3{font-size:3rem;line-height:1.3}h4{font-size:2.4rem;line-height:1.35;letter-spacing:-.08rem}h5{font-size:1.8rem;line-height:1.5;letter-spacing:-.05rem}body,h6{line-height:1.6}h6{font-size:1.5rem;letter-spacing:0}@media (min-width:550px){h1{font-size:5rem}h2{font-size:4.2rem}h3{font-size:3.6rem}h4{font-size:3rem}h5{font-size:2.4rem}h6{font-size:1.5rem}}p{margin-top:0}a{color:#1eaedb}a:hover{color:#0fa0ce}.button,button,input[type=button],input[type=reset],input[type=submit]{display:inline-block;height:38px;padding:0 30px;color:#555;text-align:center;font-size:11px;font-weight:600;line-height:38px;letter-spacing:.1rem;text-transform:uppercase;text-decoration:none;white-space:nowrap;background-color:transparent;border-radius:4px;border:1px solid #bbb;cursor:pointer;box-sizing:border-box}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{color:#333;border-color:#888;outline:0}.button.button-primary,button.button-primary,input[type=button].button-primary,input[type=reset].button-primary,input[type=submit].button-primary{color:#fff;background-color:#33c3f0;border-color:#33c3f0}.button.button-primary:focus,.button.button-primary:hover,button.button-primary:focus,button.button-primary:hover,input[type=button].button-primary:focus,input[type=button].button-primary:hover,input[type=reset].button-primary:focus,input[type=reset].button-primary:hover,input[type=submit].button-primary:focus,input[type=submit].button-primary:hover{color:#fff;background-color:#1eaedb;border-color:#1eaedb}input[type=email],input[type=number],input[type=password],input[type=search],input[type=tel],input[type=text],input[type=url],select,textarea{height:38px;padding:6px 10px;background-color:#fff;border:1px solid #d1d1d1;border-radius:4px;box-shadow:none;box-sizing:border-box}input[type=email],input[type=number],input[type=password],input[type=search],input[type=tel],input[type=text],input[type=url],textarea{-webkit-appearance:none;-moz-appearance:none;appearance:none}textarea{min-height:65px;padding-top:6px;padding-bottom:6px}input[type=email]:focus,input[type=number]:focus,input[type=password]:focus,input[type=search]:focus,input[type=tel]:focus,input[type=text]:focus,input[type=url]:focus,select:focus,textarea:focus{border:1px solid #33c3f0;outline:0}label,legend{display:block;margin-bottom:.5rem;font-weight:600}fieldset{padding:0;border-width:0}input[type=checkbox],input[type=radio]{display:inline}label>.label-body{display:inline-block;margin-left:.5rem;font-weight:400}ul{list-style:circle inside}ol{list-style:decimal inside}ol,ul{padding-left:0;margin-top:0}code,ol ol,ol ul,ul ol,ul ul{margin:1.5rem 0 1.5rem 3rem;font-size:90%}.button,button,li{margin-bottom:1rem}code{padding:.2rem .5rem;margin:0 .2rem;white-space:nowrap;background:#f1f1f1;border:1px solid #e1e1e1;border-radius:4px}pre>code{display:block;padding:1rem 1.5rem;white-space:pre}td,th{padding:12px 15px;text-align:left;border-bottom:1px solid #e1e1e1}td:first-child,th:first-child{padding-left:0}td:last-child,th:last-child{padding-right:0}fieldset,input,select,textarea{margin-bottom:1.5rem}blockquote,dl,figure,form,ol,p,pre,table,ul{margin-bottom:2.5rem}.u-full-width{width:100%;box-sizing:border-box}.u-max-full-width{max-width:100%;box-sizing:border-box}.u-pull-right{float:right}.u-pull-left{float:left}hr{margin-top:3rem;margin-bottom:3.5rem;border-width:0;border-top:1px solid #e1e1e1}.container:after,.row:after,.u-cf{content:"";display:table;clear:both}</style>
    <style>/*!
 * Toastify js 1.12.0
 * https://github.com/apvarun/toastify-js
 * @license MIT licensed
 *
 * Copyright (C) 2018 Varun A P
 */
.toastify{padding:12px 20px;color:#ff9734;display:inline-block;box-shadow:0 3px 6px -1px rgba(0,0,0,.12),0 10px 36px -4px rgba(77,96,232,.3);background:-webkit-linear-gradient(315deg,#73a5ff,#5477f5);background:linear-gradient(135deg,#73a5ff,#5477f5);position:fixed;opacity:0;transition:all .4s cubic-bezier(.215,.61,.355,1);border-radius:2px;cursor:pointer;text-decoration:none;max-width:calc(50% - 20px);z-index:2147483647}.toastify.on{opacity:1}.toast-close{background:0 0;border:0;color:#fff;cursor:pointer;font-family:inherit;font-size:1em;opacity:.4;padding:0 5px}.toastify-right{right:15px}.toastify-left{left:15px}.toastify-top{top:-150px}.toastify-bottom{bottom:-150px}.toastify-rounded{border-radius:25px}.toastify-avatar{width:1.5em;height:1.5em;margin:-7px 5px;border-radius:2px}.toastify-center{margin-left:auto;margin-right:auto;left:0;right:0;max-width:fit-content;max-width:-moz-fit-content}@media only screen and (max-width:360px){.toastify-left,.toastify-right{margin-left:auto;margin-right:auto;left:0;right:0;max-width:fit-content}}</style>
    <style>
    label,summary{font-weight:600;font-size:0.85rem;text-transform:uppercase;letter-spacing:0.5px;color:rgba(255,255,255,0.8);margin-bottom:0.4rem;margin-top:0.5rem;}
    form,h3{margin-bottom:1.5rem}
    .dot_green,.dot_red{height:10px;width:10px;border-radius:50%;display:inline-block;margin-right:6px;box-shadow:0 0 5px rgba(0,0,0,0.5);}
    html{min-height:100%}
    body{height:100%;margin:0;font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;color:#fff;background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);background-attachment:fixed;}
    h2{margin-top:0;font-size:2rem;font-weight:300;border-bottom:1px solid rgba(255,255,255,0.15);padding-bottom:0.5rem;margin-bottom:1.5rem;letter-spacing:1px;}
    h3,h4{font-weight:300;margin-top:1.5rem;}
    button.button-primary{background:linear-gradient(135deg,#ff9734,#e6862b);color:#fff;border:none;border-radius:8px;padding:0 25px;height:44px;line-height:44px;font-size:1rem;transition:all 0.3s ease;box-shadow:0 4px 10px rgba(0,0,0,0.2);cursor:pointer;font-weight:600;width:100%;text-transform:uppercase;letter-spacing:1px;}
    button.button-primary:hover{transform:translateY(-2px);box-shadow:0 6px 15px rgba(0,0,0,0.3);color:#fff;background:linear-gradient(135deg,#e6862b,#cc7421);}
    input,select{color:#000;background:rgba(255,255,255,0.95);border:1px solid rgba(255,255,255,0.3);border-radius:6px;padding:10px 12px;font-size:1rem;transition:all 0.3s;width:100%;box-sizing:border-box;font-family:inherit;}
    input:focus,select:focus{background:#fff;border-color:#ff9734;box-shadow:0 0 8px rgba(255,151,52,0.5);outline:none;}
    .dot_green{background-color:#68b838;}
    .dot_red{background-color:#f63e3e;}
    .small_text{font-size:0.95rem;color:rgba(255,255,255,0.9);}
    .img_button{transition:all 0.2s;cursor:pointer;}
    .img_button:hover{transform:scale(1.2);filter:invert(60%) sepia(100%) saturate(509%) hue-rotate(334deg) brightness(101%) contrast(101%);}
    .tooltip{position:relative;display:inline-block;}
    .tooltip .tooltiptext{visibility:hidden;width:max-content;max-width:250px;background-color:rgba(0,0,0,0.85);color:#fff;text-align:center;padding:8px 12px;border-radius:6px;position:absolute;z-index:10;bottom:130%;left:50%;transform:translateX(-50%);opacity:0;transition:opacity 0.3s;font-size:0.85rem;font-weight:normal;line-height:1.4;}
    .tooltip .tooltiptext::after{content:"";position:absolute;top:100%;left:50%;margin-left:-5px;border-width:5px;border-style:solid;border-color:rgba(0,0,0,0.85) transparent transparent;}
    .tooltip:hover .tooltiptext{visibility:visible;opacity:1;}
    div.info{background:rgba(0,0,0,0.25);border-radius:8px;padding:15px;margin-bottom:1.5rem;box-shadow:0 4px 6px rgba(0,0,0,0.1);backdrop-filter:blur(5px);border:1px solid rgba(255,255,255,0.05);}
    div.note{border-left:solid 4px #ff881a;}
    div.issue{border-left:solid 4px #f63e3e;}
    .container > form > div {background:rgba(0,0,0,0.35);padding:2rem;border-radius:16px;margin-bottom:2rem;box-shadow:0 8px 32px rgba(0,0,0,0.3);backdrop-filter:blur(12px);border:1px solid rgba(255,255,255,0.05);}
    img[alt="Bridge Logo"] {max-height:60px;filter:drop-shadow(0 4px 6px rgba(0,0,0,0.3));margin-top:1rem;margin-bottom:1rem;display:block;margin-left:auto;margin-right:auto;}
    .row {margin-bottom: 1rem;}
    #read_bytes, #tcp_connected, #udp_connected, #wifi_rssi_dbm {font-size:1.1rem;font-weight:500;padding:8px 0;}
    #web_conn_status {font-size:1.1rem;font-weight:600;padding:10px;background:rgba(0,0,0,0.2);border-radius:8px;text-align:center;margin-bottom:1rem;width:100%;}
    #current_client_ip {color:rgba(255,255,255,0.7);font-size:0.9rem;}
    
    @supports (-webkit-appearance:none) or (-moz-appearance:none){.checkbox-wrapper-14 input[type=checkbox]{--active:#ff9734;--active-inner:#fff;--focus:0px rgb(255, 169, 88);--border:#ffffff;--border-hover:#ff9734;--background:#fffff;--disabled:#F6F8FF;--disabled-inner:#E1E6F9;-webkit-appearance:none;-moz-appearance:none;height:24px;outline:0;display:inline-block;vertical-align:top;position:relative;cursor:pointer;border:1px solid var(--bc, var(--border));background:var(--b, var(--background));transition:background .3s,border-color .3s,box-shadow .2s}.checkbox-wrapper-14 input[type=checkbox]:after{content:"";display:block;left:0;top:0;position:absolute;transition:transform var(--d-t, 0.3s) var(--d-t-e, ease),opacity var(--d-o, 0.2s)}.checkbox-wrapper-14 input[type=checkbox]:checked{--b:var(--active);--bc:var(--active);--d-o:.3s;--d-t:.6s;--d-t-e:cubic-bezier(.2, .85, .32, 1.2)}.checkbox-wrapper-14 input[type=checkbox]:disabled{--b:var(--disabled);cursor:not-allowed;opacity:.9}.checkbox-wrapper-14 input[type=checkbox]:disabled:checked{--b:var(--disabled-inner);--bc:var(--border)}.checkbox-wrapper-14 input[type=checkbox]:disabled+label{cursor:not-allowed}.checkbox-wrapper-14 input[type=checkbox]:hover:not(:checked):not(:disabled){--bc:var(--border-hover)}.checkbox-wrapper-14 input[type=checkbox]:focus{--bc:var(--active)}.checkbox-wrapper-14 input[type=checkbox]:not(.switch){width:24px;border-radius:7px}.checkbox-wrapper-14 input[type=checkbox]:not(.switch):after{opacity:var(--o, 0);width:6px;height:10px;border:2px solid var(--active-inner);border-top:0;border-left:0;left:8px;top:5px;transform:rotate(var(--r, 20deg))}.checkbox-wrapper-14 input[type=checkbox]:not(.switch):checked{--o:1;--r:43deg}.checkbox-wrapper-14 input[type=checkbox]+label{display:inline-block;vertical-align:middle;cursor:pointer;margin-left:8px;font-weight:500;font-size:1rem;color:#fff;text-transform:none;letter-spacing:normal;margin-top:2px;}.checkbox-wrapper-14 input[type=checkbox].switch{width:44px;border-radius:12px}.checkbox-wrapper-14 input[type=checkbox].switch:after{left:3px;top:3px;border-radius:50%;width:16px;height:16px;background:var(--ab, var(--border));transform:translateX(var(--x, 0))}.checkbox-wrapper-14 input[type=checkbox].switch:checked{--ab:var(--active-inner);--x:20px}.checkbox-wrapper-14 input[type=checkbox].switch:disabled:not(:checked):after{opacity:.6}}.checkbox-wrapper-14 *{box-sizing:inherit}.checkbox-wrapper-14 :after,.checkbox-wrapper-14 :before{box-sizing:inherit}
    </style>
    <script>/*!
 * Toastify js 1.12.0
 * https://github.com/apvarun/toastify-js
 * @license MIT licensed
 *
 * Copyright (C) 2018 Varun A P
 */
!function(t,o){"object"==typeof module&&module.exports?module.exports=o():t.Toastify=o()}(this,(function(t){var o=function(t){return new o.lib.init(t)};function i(t,o){return o.offset[t]?isNaN(o.offset[t])?o.offset[t]:o.offset[t]+"px":"0px"}function s(t,o){return!(!t||"string"!=typeof o)&&!!(t.className&&t.className.trim().split(/\s+/gi).indexOf(o)>-1)}return o.defaults={oldestFirst:!0,text:"Toastify is awesome!",node:void 0,duration:3e3,selector:void 0,callback:function(){},destination:void 0,newWindow:!1,close:!1,gravity:"toastify-top",positionLeft:!1,position:"",backgroundColor:"",avatar:"",className:"",stopOnFocus:!0,onClick:function(){},offset:{x:0,y:0},escapeMarkup:!0,ariaLive:"polite",style:{background:""}},o.lib=o.prototype={toastify:"1.12.0",constructor:o,init:function(t){return t||(t={}),this.options={},this.toastElement=null,this.options.text=t.text||o.defaults.text,this.options.node=t.node||o.defaults.node,this.options.duration=0===t.duration?0:t.duration||o.defaults.duration,this.options.selector=t.selector||o.defaults.selector,this.options.callback=t.callback||o.defaults.callback,this.options.destination=t.destination||o.defaults.destination,this.options.newWindow=t.newWindow||o.defaults.newWindow,this.options.close=t.close||o.defaults.close,this.options.gravity="bottom"===t.gravity?"toastify-bottom":o.defaults.gravity,this.options.positionLeft=t.positionLeft||o.defaults.positionLeft,this.options.position=t.position||o.defaults.position,this.options.backgroundColor=t.backgroundColor||o.defaults.backgroundColor,this.options.avatar=t.avatar||o.defaults.avatar,this.options.className=t.className||o.defaults.className,this.options.stopOnFocus=void 0===t.stopOnFocus?o.defaults.stopOnFocus:t.stopOnFocus,this.options.onClick=t.onClick||o.defaults.onClick,this.options.offset=t.offset||o.defaults.offset,this.options.escapeMarkup=void 0!==t.escapeMarkup?t.escapeMarkup:o.defaults.escapeMarkup,this.options.ariaLive=t.ariaLive||o.defaults.ariaLive,this.options.style=t.style||o.defaults.style,t.backgroundColor&&(this.options.style.background=t.backgroundColor),this},buildToast:function(){if(!this.options)throw"Toastify is not initialized";var t=document.createElement("div");for(var o in t.className="toastify on "+this.options.className,this.options.position?t.className+=" toastify-"+this.options.position:!0===this.options.positionLeft?(t.className+=" toastify-left",console.warn("Property `positionLeft` will be depreciated in further versions. Please use `position` instead.")):t.className+=" toastify-right",t.className+=" "+this.options.gravity,this.options.backgroundColor&&console.warn('DEPRECATION NOTICE: "backgroundColor" is being deprecated. Please use the "style.background" property.'),this.options.style)t.style[o]=this.options.style[o];if(this.options.ariaLive&&t.setAttribute("aria-live",this.options.ariaLive),this.options.node&&this.options.node.nodeType===Node.ELEMENT_NODE)t.appendChild(this.options.node);else if(this.options.escapeMarkup?t.innerText=this.options.text:t.innerHTML=this.options.text,""!==this.options.avatar){var s=document.createElement("img");s.src=this.options.avatar,s.className="toastify-avatar","left"==this.options.position||!0===this.options.positionLeft?t.appendChild(s):t.insertAdjacentElement("afterbegin",s)}if(!0===this.options.close){var e=document.createElement("button");e.type="button",e.setAttribute("aria-label","Close"),e.className="toast-close",e.innerHTML="&#10006;",e.addEventListener("click",function(t){t.stopPropagation(),this.removeElement(this.toastElement),window.clearTimeout(this.toastElement.timeOutValue)}.bind(this));var n=window.innerWidth>0?window.innerWidth:screen.width;("left"==this.options.position||!0===this.options.positionLeft)&&n>360?t.insertAdjacentElement("afterbegin",e):t.appendChild(e)}if(this.options.stopOnFocus&&this.options.duration>0){var a=this;t.addEventListener("mouseover",(function(o){window.clearTimeout(t.timeOutValue)})),t.addEventListener("mouseleave",(function(){t.timeOutValue=window.setTimeout((function(){a.removeElement(t)}),a.options.duration)}))}if(void 0!==this.options.destination&&t.addEventListener("click",function(t){t.stopPropagation(),!0===this.options.newWindow?window.open(this.options.destination,"_blank"):window.location=this.options.destination}.bind(this)),"function"==typeof this.options.onClick&&void 0===this.options.destination&&t.addEventListener("click",function(t){t.stopPropagation(),this.options.onClick()}.bind(this)),"object"==typeof this.options.offset){var l=i("x",this.options),r=i("y",this.options),p="left"==this.options.position?l:"-"+l,d="toastify-top"==this.options.gravity?r:"-"+r;t.style.transform="translate("+p+","+d+")"}return t},showToast:function(){var t;if(this.toastElement=this.buildToast(),!(t="string"==typeof this.options.selector?document.getElementById(this.options.selector):this.options.selector instanceof HTMLElement||"undefined"!=typeof ShadowRoot&&this.options.selector instanceof ShadowRoot?this.options.selector:document.body))throw"Root element is not defined";var i=o.defaults.oldestFirst?t.firstChild:t.lastChild;return t.insertBefore(this.toastElement,i),o.reposition(),this.options.duration>0&&(this.toastElement.timeOutValue=window.setTimeout(function(){this.removeElement(this.toastElement)}.bind(this),this.options.duration)),this},hideToast:function(){this.toastElement.timeOutValue&&clearTimeout(this.toastElement.timeOutValue),this.removeElement(this.toastElement)},removeElement:function(t){t.className=t.className.replace(" on",""),window.setTimeout(function(){this.options.node&&this.options.node.parentNode&&this.options.node.parentNode.removeChild(this.options.node),t.parentNode&&t.parentNode.removeChild(t),this.options.callback.call(t),o.reposition()}.bind(this),400)}},o.reposition=function(){for(var t,o={top:15,bottom:15},i={top:15,bottom:15},e={top:15,bottom:15},n=document.getElementsByClassName("toastify"),a=0;a<n.length;a++){t=!0===s(n[a],"toastify-top")?"toastify-top":"toastify-bottom";var l=n[a].offsetHeight;t=t.substr(9,t.length-1);(window.innerWidth>0?window.innerWidth:screen.width)<=360?(n[a].style[t]=e[t]+"px",e[t]+=l+15):!0===s(n[a],"toastify-left")?(n[a].style[t]=o[t]+"px",o[t]+=l+15):(n[a].style[t]=i[t]+"px",i[t]+=l+15)}return this},o.lib.init.prototype=o.lib,o}));</script>
    <script>const ROOT_URL=window.location.origin+"/";let conn_status=0,old_conn_status=0,conn_fail_count=0,CONN_FAIL_THRESHOLD=3,serial_via_JTAG=0,last_byte_count=0,last_timestamp_byte_count=0,esp_chip_model=0,recv_ser_bytes=0,serial_dec_mav_msgs=0,set_telem_proto=null;function change_radio_dis_arm_visibility(){let e=document.getElementById("radio_dis_onarm_div");document.getElementById("esp32_mode").value>"2"&&document.getElementById("esp32_mode").value<"6"||"5"===document.getElementById("proto").value?e.style.display="none":e.style.display="block"}function change_ap_ip_visibility(){const e=document.getElementById("esp32_mode").value,t={ap_ip_div:document.getElementById("ap_ip_div"),ap_channel_div:document.getElementById("ap_channel_div"),lr_disclaimer_div:document.getElementById("esp-lr-ap-disclaimer"),ble_disclaimer_div:document.getElementById("ble_disclaimer_div"),wifi_ssid_div:document.getElementById("wifi_ssid_div"),wifi_en_gn_div:document.getElementById("wifi_en_gn_div"),static_ip_config_div:document.getElementById("static_ip_config_div"),pass_div:document.getElementById("pass_div")};"2"===e?(t.ap_ip_div.style.display="none",t.ap_channel_div.style.display="none",t.wifi_en_gn_div.style.display="block",t.static_ip_config_div.style.display="block"):(t.ap_ip_div.style.display="block",t.ap_channel_div.style.display="block",t.wifi_en_gn_div.style.display="none",t.static_ip_config_div.style.display="none"),"6"===e?(t.ble_disclaimer_div.style.display="block",t.wifi_ssid_div.style.display="none",t.ap_channel_div.style.display="none",t.pass_div.style.display="none",t.ap_ip_div.style.display="none"):(t.ble_disclaimer_div.style.display="none",t.wifi_ssid_div.style.display="block",t.pass_div.style.display="block"),t.lr_disclaimer_div.style.display=e>"2"&&e<"6"?"block":"none",e>"3"&&e<"6"?(t.ap_ip_div.style.display="none",t.wifi_ssid_div.style.visibility="hidden"):t.wifi_ssid_div.style.visibility="visible",change_radio_dis_arm_visibility()}function change_msp_ltm_visibility(){let e=document.getElementById("msp_ltm_div"),t=document.getElementById("trans_pack_size_div"),n=document.getElementById("rep_rssi_dbm_div"),s=document.getElementById("proto");"1"===s.value?(e.style.display="block",t.style.display="none"):(e.style.display="none",t.style.display="block"),"4"===s.value?n.style.display="block":n.style.display="none",change_radio_dis_arm_visibility()}function change_uart_visibility(){let e=document.getElementById("tx_rx_div"),t=document.getElementById("rts_cts_div"),n=document.getElementById("rts_thresh_div"),s=document.getElementById("baud_div");0===serial_via_JTAG?(t.style.display="block",e.style.display="block",n.style.display="block",s.style.display="block"):(t.style.display="none",e.style.display="none",n.style.display="none",s.style.display="none")}function flow_control_check(){let e=document.getElementById("gpio_rts"),t=document.getElementById("gpio_cts");isNaN(e.value)||isNaN(t.value)||""===t.value||""===e.value||e.value===t.value?show_toast("Управление потоком UART отключено."):show_toast("Управление потоком UART включено. Подключите выводы RTS и CTS!")}function toJSONString(e){let t={},n=e.querySelectorAll("input, select");for(let e=0;e<n.length;++e){let s=n[e],i=s.name,o=s.value;isNaN(Number(o))||0===i.localeCompare("wifi_ssid")||0===i.localeCompare("wifi_pass")?i&&("checkbox"===s.type?t[i]=s.checked?1:0:t[i]=o):i&&(t[i]=parseInt(o))}return JSON.stringify(t)}async function get_json(e){let t=ROOT_URL+e;const n=new AbortController,s=(setTimeout(()=>{n.abort()},2500),await fetch(t,{signal:n.signal}));if(!s.ok){const e="Ошибка: "+s.status;throw conn_fail_count++,conn_fail_count>=CONN_FAIL_THRESHOLD&&(conn_status=0),new Error(e)}conn_fail_count=0;conn_status=1;return await s.json()}async function send_json(e,t){let n=ROOT_URL+e;const s=await fetch(n,{method:"POST",headers:{Accept:"application/json","Content-Type":"application/json",charset:"utf-8"},body:t});if(!s.ok){conn_fail_count++;conn_fail_count>=CONN_FAIL_THRESHOLD&&(conn_status=0);const e="Ошибка: "+s.status;throw new Error(e)}conn_fail_count=0;conn_status=1;return await s.json()}function get_esp_chip_model_str(e){switch(e){default:case 0:return"unknown/unsupported ESP32 chip";case 1:return"ESP32";case 2:return"ESP32-S2";case 9:return"ESP32-S3";case 5:return"ESP32-C3";case 13:return"ESP32-C6";case 12:return"ESP32-C5"}}function get_system_info(){return get_json("api/system/info").then(e=>{console.log("Received settings: "+e),document.getElementById("about").innerHTML="Bridge for ESP32 v"+e.major_version+"."+e.minor_version+"."+e.patch_version+" ("+e.maturity_version+") - esp-idf "+e.idf_version+" - "+get_esp_chip_model_str(e.esp_chip_model),document.getElementById("esp_mac").innerHTML=e.esp_mac,serial_via_JTAG=e.serial_via_JTAG,1===parseInt(e.has_rf_switch)?document.getElementById("ant_use_ext_div").style.display="block":document.getElementById("ant_use_ext_div").style.display="none"}).catch(e=>(conn_status=0,e.message,-1)),0}function update_conn_status(){conn_status?document.getElementById("web_conn_status").innerHTML='<span class="dot_green"></span> подключено к ESP32':(document.getElementById("web_conn_status").innerHTML='<span class="dot_red"></span> нет связи с ESP32',document.getElementById("current_client_ip").innerHTML=""),conn_status!==old_conn_status&&(get_system_info(),get_settings(),setTimeout(change_msp_ltm_visibility,500),setTimeout(change_ap_ip_visibility,500),setTimeout(change_uart_visibility,500)),old_conn_status=conn_status}function get_stats(){get_json("api/system/stats").then(e=>{conn_status=1;let t=new Date;recv_ser_bytes=parseInt(e.read_bytes),serial_dec_mav_msgs=parseInt(e.serial_dec_mav_msgs);let n=0,s=t.getTime();last_byte_count>0&&last_timestamp_byte_count>0&&!isNaN(recv_ser_bytes)&&(n=(recv_ser_bytes-last_byte_count)/((s-last_timestamp_byte_count)/1e3)),last_timestamp_byte_count=s,!isNaN(recv_ser_bytes)&&recv_ser_bytes>1e6?document.getElementById("read_bytes").innerHTML=(recv_ser_bytes/1e6).toFixed(3)+" MB ("+(8*n/1e3).toFixed(2)+" kbit/s)":!isNaN(recv_ser_bytes)&&recv_ser_bytes>1e3?document.getElementById("read_bytes").innerHTML=(recv_ser_bytes/1e3).toFixed(2)+" kB ("+(8*n/1e3).toFixed(2)+" kbit/s)":isNaN(recv_ser_bytes)||(document.getElementById("read_bytes").innerHTML=recv_ser_bytes+" bytes ("+Math.round(n)+" byte/s)"),last_byte_count=recv_ser_bytes;let i=parseInt(e.tcp_connected);isNaN(i)||1!==i?isNaN(i)||(document.getElementById("tcp_connected").innerHTML=i+" клиентов"):document.getElementById("tcp_connected").innerHTML=i+" клиент";let o="";if(e.hasOwnProperty("udp_clients")){let t=e.udp_clients;for(let e=0;e<t.length;e++)o+=t[e],e+1!==t.length&&(o+="<br>");0===t.length?o="-":document.getElementById("tooltip_udp_clients").innerHTML=o}let l=parseInt(e.udp_connected);if(isNaN(l)||1!==l?isNaN(l)||(document.getElementById("udp_connected").innerHTML='<span class="tooltiptext" id="tooltip_udp_clients">'+o+"</span>"+l+" клиентов"):document.getElementById("udp_connected").innerHTML='<span class="tooltiptext" id="tooltip_udp_clients">'+o+"</span>"+l+" клиент","esp_rssi"in e){let t=parseInt(e.esp_rssi);var rEl=document.getElementById("wifi_rssi_dbm");if(rEl)!isNaN(t)&&t!==0?rEl.textContent=t+" dBm":rEl.textContent="— dBm";!isNaN(t)&&t<0?document.getElementById("current_client_ip").innerHTML="IP-адрес: "+e.current_client_ip+"<br />Уровень сигнала: "+t+" dBm":isNaN(t)||(document.getElementById("current_client_ip").innerHTML="IP-адрес: "+e.current_client_ip)}else if("connected_sta"in e){let t="";e.connected_sta.forEach(e=>{t=t+"Клиент: "+e.sta_mac+" Уровень сигнала: "+e.sta_rssi+"dBm<br />"}),document.getElementById("current_client_ip").innerHTML=t}}).catch(e=>{conn_fail_count++;conn_fail_count>=CONN_FAIL_THRESHOLD&&(conn_status=0);e.message})}function get_settings(){return get_json("api/settings").then(e=>{console.log("Received settings: "+e),conn_status=1;for(const t in e)if(e.hasOwnProperty(t)){let n=document.getElementById(t);null!=n&&("checkbox"===n.type?n.checked=1===e[t]:n.value=e[t]+"")}set_telem_proto=document.getElementById("proto").value}).catch(e=>(conn_status=0,e.message,show_toast(e.message),-1)),change_ap_ip_visibility(),change_msp_ltm_visibility(),0}function add_new_udp_client(){let e=prompt("Введите IP-адрес UDP-приёмника","192.168.2.X");if(null==e)return void show_toast("Отменено пользователем.");let t=prompt("Введите номер порта UDP-приёмника","14550");if(null==t)return void show_toast("Отменено пользователем.");t=parseInt(t);let n=confirm("Сохранить этот UDP-клиент в память (будет подключаться после перезагрузки)?\nМожно сохранить только один. Выберите «Нет», если нужно добавить только на эту сессию.");if(null!=e&&null!=t&&/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(e)){let s={udp_client_ip:e,udp_client_port:t,save:n};send_json("api/settings/clients/udp",JSON.stringify(s)).then(e=>{console.log(e),conn_status=1,show_toast(e.msg)}).catch(e=>{show_toast(e.message)})}else show_toast("Ошибка: введите правильный IP и порт!")}async function clear_udp_clients(){if(!0===confirm("Удалить все UDP-подключения? GCS придётся подключаться заново.")){let e=ROOT_URL+"api/settings/clients/clear_udp";const t=await fetch(e,{method:"DELETE",headers:{Accept:"application/json","Content-Type":"application/json",charset:"UTF-8"},body:null});if(!t.ok){conn_status=0;const e="Ошибка: "+t.status;throw new Error(e)}else{show_toast("UDP-клиент сброшен.");get_stats();}}}function show_toast(e,t="#0058a6"){Toastify({text:e,duration:5e3,newWindow:!0,close:!0,gravity:"top",position:"center",style:{background:t,color:"#ff9734",borderColor:"#ff9734",borderStyle:"solid",borderRadius:"2px",borderWidth:"1px"},stopOnFocus:!0}).showToast()}function check_validity(){let e=!0;return document.getElementById("wifi_pass").checkValidity()||(show_toast("Ошибка: пароль от 8 до 64 символов"),e=!1),e}function check_for_issues(){let e=document.getElementById("issue_div");e.style.display="4"===set_telem_proto&&0===serial_dec_mav_msgs&&0!==recv_ser_bytes?"block":"none"}function save_settings(){let e=document.getElementById("settings_form");if(check_validity()){send_json("api/settings",toJSONString(e)).then(e=>{console.log(e),conn_status=1,show_toast(e.msg),get_settings()}).catch(e=>{show_toast(e.message)})}else console.log("Form was not filled out correctly.")}</script>
</head>
<body>
<div class="container">
    <br>
    <div class="twelve columns">
        <img src="DroneBridgeLogo.png" alt="Bridge Logo">
    </div>
</div>
<div class="container">
    <form id="settings_form">
        <div>
            <div class="info issue" id="issue_div">
                Возможна ошибка настройки скорости UART.<br>Совместите скорости на полётном контроллере и ESP32.<br>ESP32 получает данные по UART, но не может декодировать MAVLink.
            </div>
            <h2>Статистика</h2>
            <div class="row">
                <div id="web_conn_status" class="six columns small_text">
                </div>
            </div>
            <div class="row">
                <div id="current_client_ip" class="twelve columns small_text">
                </div>
            </div>
            <div class="row">
                <div class="six columns small_text">
                    <label>Мощность сигнала WiFi</label>
                    <div id="wifi_rssi_dbm">— dBm</div>
                </div>
            </div>
            <div class="row">
                <div class="four columns">
                    <label>Принято байт (последовательный порт)</label>
                    <div id="read_bytes">-</div>
                </div>
                <div class="four columns">
                    <label>Подключённые TCP-клиенты</label>
                    <div id="tcp_connected">-</div>
                </div>
                <div class="four columns">
                    <label>Подключённые UDP-клиенты</label>
                    <div style="display: flex" >
                        <div id="udp_connected" class="tooltip" style="width: fit-content"><span class="tooltiptext" id="tooltip_udp_clients">-</span>-</div>
                        <img class="img_button" style="margin-left: 5rem; padding: 0.2rem" height="20em" alt="add" src="add_16dp_icon.png" onclick="add_new_udp_client()"/>
                        <img class="img_button" style="margin-left: 0.5rem; padding: 0.2rem" height="20em" alt="delete" src="remove_16dp_icon.png" onclick="clear_udp_clients()"/>
                    </div>
                </div>
            </div>
            <div class="row">
                <div class="twelve columns">
                    <label>Метрики канала (MAVLink)</label>
                    <div id="link_metrics" class="small_text">-</div>
                </div>
            </div>
        </div>
        <div>
            <h2>Настройки</h2>
            <h3>Wi‑Fi</h3>
            <div class="row">
                <div class="twelve columns">
                    <label for="esp32_mode">Режим ESP32</label>
                    <select id="esp32_mode" name="esp32_mode" form="settings_form" onchange="change_ap_ip_visibility()">
                        <option value="1">Точка доступа Wi‑Fi (AP)</option>
                        <option value="2">Клиент Wi‑Fi (STA)</option>
                        <option value="3">Точка доступа Wi‑Fi LR</option>
                        <option value="4">ESP-NOW LR — воздух</option>
                        <option value="5">ESP-NOW LR — земля</option>
                        <option value="6">Bluetooth LE</option>
                    </select>
                    <div class="info note" id="esp-lr-ap-disclaimer"><h4>Внимание:</h4>
                        Режим LR делает устройство невидимым для обычных устройств!<br>Изменить настройки будет нельзя!<br><br>
                        Скорость снижается до ~0,25 Мбит/с. Дальность может вырасти примерно в 2 раза.<br>Нажмите кнопку BOOT
                        (короткое нажатие — режим AP с паролем «bridge», длинное — сброс по умолчанию)
                        или очистите память ESP32 и прошейте заново, чтобы вернуться в
                        обычный режим Wi‑Fi!
                    </div>
                    <div class="info note" id="ble_disclaimer_div"><h4>Внимание:</h4>
                        <b>БЕТА: Bluetooth LE (BLE) в тестировании.</b><br>
                        Программа (GCS, Betaflight Configurator) должна поддерживать подключение по BLE.<br>
                        Пока только последний Betaflight Configurator (BFC) поддерживает BLE.<br><br>
                        Используйте Bridge BLE для подключения QGC, Mission Planner или BFC к ESP32.<br>
                        Настройки точки доступа Wi‑Fi будут использоваться параллельно для доступа к этой странице.
                    </div>
                </div>
            </div>
            <div class="row">
                <div class="six columns" id="wifi_ssid_div">
                    <label for="ssid">Имя сети (SSID)</label>
                    <input type="text" name="ssid" value="" id="ssid" maxlength="31">
                </div>
                <div class="six columns" id="pass_div">
                    <label for="wifi_pass">Пароль</label>
                    <input type="text" name="wifi_pass" value="" id="wifi_pass" minlength="8" maxlength="63" required>
                </div>
            </div>
            <div class="row">
                <div id="ap_channel_div" class="six columns">
                    <label for="wifi_chan">Канал</label>
                    <select id="wifi_chan" name="wifi_chan" form="settings_form">
                        <option value="1">1</option>
                        <option value="2">2</option>
                        <option value="3">3</option>
                        <option value="4">4</option>
                        <option value="5">5</option>
                        <option value="6">6</option>
                        <option value="7">7</option>
                        <option value="8">8</option>
                        <option value="9">9</option>
                        <option value="10">10</option>
                        <option value="11">11</option>
                        <option value="12">12</option>
                        <option value="13">13</option>
                    </select>
                </div>
                <div class="six columns" id="ap_ip_div">
                    <label for="ap_ip">IP-адрес шлюза (AP)</label>
                    <input type="text" name="ap_ip" value="" id="ap_ip">
                </div>
            </div>
            <div id="wifi_en_gn_div" class="row">
                <div class="twelve columns">
                    <div class="checkbox-wrapper-14">
                        <input id="wifi_en_gn" name="wifi_en_gn" type="checkbox" class="switch">
                        <label class="tooltip" for="wifi_en_gn"><span class="tooltiptext">Включайте только при необходимости b/g/n/ax. В выключенном состоянии в режиме клиента используется только 802.11b — достаточно для телеметрии и большей дальности. 802.11ax только на поддерживаемых чипах.</span>Поддержка 802.11b/g/n/ax</label>
                    </div>
                </div>
            </div>
            <div id="radio_dis_onarm_div" class="row">
                <div class="twelve columns">
                    <div class="checkbox-wrapper-14">
                        <input id="radio_dis_onarm" name="radio_dis_onarm" type="checkbox" class="switch">
                        <label for="radio_dis_onarm">Отключать радиомодуль при взлёте автопилота</label>
                    </div>
                </div>
            </div>
            <div id="ant_use_ext_div" class="row">
                <div class="twelve columns">
                    <div class="checkbox-wrapper-14">
                        <input id="ant_use_ext" name="ant_use_ext" type="checkbox" class="switch">
                        <label for="ant_use_ext">Внешняя антенна</label>
                    </div>
                </div>
            </div>
            <div id="static_ip_config_div">
                <details>
                    <summary>Дополнительные настройки Wi‑Fi</summary>
                    <div class="row">
                        <div class="six columns" id="ip_sta_div">
                            <label for="ip_sta">Статический IP-адрес</label>
                            <input type="text" name="ip_sta" value="" id="ip_sta" maxlength="15" placeholder="необязательно">
                        </div>
                        <div class="six columns" id="ip_sta_gw_div">
                            <label for="ip_sta_gw">Адрес шлюза</label>
                            <input type="text" name="ip_sta_gw" value="" id="ip_sta_gw" maxlength="15" placeholder="необязательно">
                        </div>
                    </div>
                    <div class="row">
                        <div class="six columns" id="ip_sta_netmsk_div">
                            <label for="ip_sta_netmsk">Маска подсети</label>
                            <input type="text" name="ip_sta_netmsk" value="" id="ip_sta_netmsk" maxlength="15" placeholder="необязательно">
                        </div>
                        <div class="six columns" id="wifi_hostname_div">
                            <label for="wifi_hostname">Имя хоста (mDNS)</label>
                            <input type="text" name="wifi_hostname" value="" maxlength="31" id="wifi_hostname" placeholder="Bridge_ESP32">
                        </div>
                    </div>
                </details>
            </div>
            <h3 style="margin-top: 2rem">Последовательный порт (UART)</h3>
            <div class="row" id="tx_rx_div">
                <div class="six columns">
                    <label for="gpio_tx">UART TX (GPIO)</label>
                    <input type="number" name="gpio_tx" value="" id="gpio_tx">
                </div>
                <div class="six columns">
                    <label for="gpio_rx">UART RX (GPIO)</label>
                    <input type="number" name="gpio_rx" value="" id="gpio_rx">
                </div>
            </div>
            <div class="row" id="rts_cts_div">
                <div class="six columns">
                    <label for="gpio_rts">UART RTS (GPIO)</label>
                    <input type="number" name="gpio_rts" value="" id="gpio_rts" onchange="flow_control_check()">
                </div>
                <div class="six columns">
                    <label for="gpio_cts">UART CTS (GPIO)</label>
                    <input type="number" name="gpio_cts" value="" id="gpio_cts" onchange="flow_control_check()">
                </div>
            </div>
            <div class="row" id="rts_thresh_div">
                <div class="six columns">
                    <label for="rts_thresh">Порог UART RTS</label>
                    <input type="number" id="rts_thresh" name="rts_thresh" min="1" max="128">
                </div>
            </div>
            <div class="row">
                <div class="six columns">
                    <label for="proto">Протокол UART</label>
                    <select id="proto" name="proto" form="settings_form"
                            onchange="change_msp_ltm_visibility()">
                        <option value="1">MSP/LTM</option>
                        <option value="4">MAVLink</option>
                        <option value="5">Transparent</option>
                    </select>
                </div>
                <div class="six columns" id="baud_div">
                    <label for="baud">Скорость UART (бод)</label>
                    <select name="baud" id="baud" form="settings_form">
                        <option value="5000000">5000000</option>
                        <option value="3000000">3000000</option>
                        <option value="2000000">2000000</option>
                        <option value="1500000">1500000</option>
                        <option value="1000000">1000000</option>
                        <option value="921600">921600</option>
                        <option value="576000">576000</option>
						<option value="500800">500800</option>
                        <option value="460800">460800</option>
                        <option value="230400">230400</option>
                        <option value="115200">115200</option>
                        <option value="76800">76800</option>
                        <option value="57600">57600</option>
                        <option value="38400">38400</option>
                        <option value="19200">19200</option>
                        <option value="9600">9600</option>
                        <option value="4800">4800</option>
                        <option value="2400">2400</option>
                        <option value="1200">1200</option>
                    </select>
                </div>
            </div>
            <div id="msp_ltm_div" class="row">
                <div class="twelve columns">
                    <label for="ltm_per_packet">LTM кадров на пакет</label>
                    <select id="ltm_per_packet" name="ltm_per_packet" form="settings_form">
                        <option value="1">1</option>
                        <option value="2">2</option>
                        <option value="3">3</option>
                        <option value="4">4</option>
                        <option value="5">5</option>
                    </select>
                </div>
            </div>
            <div id="trans_pack_size_div" class="row">
                <div class="six columns">
                    <label for="trans_pack_size">Макс. размер пакета</label>
                    <select id="trans_pack_size" name="trans_pack_size" form="settings_form">
                        <option value="16">16</option>
                        <option value="32">32</option>
                        <option value="64">64</option>
                        <option value="96">96</option>
                        <option value="128">128</option>
                        <option value="160">160</option>
                        <option value="192">192</option>
                        <option value="224">224</option>
                        <option value="256">256</option>
                        <option value="288">288</option>
                        <option value="320">320</option>
                        <option value="352">352</option>
                        <option value="384">384</option>
                        <option value="416">416</option>
                        <option value="448">448</option>
                        <option value="480">480</option>
                        <option value="512">512</option>
                        <option value="544">544</option>
                        <option value="576">576</option>
                        <option value="608">608</option>
                        <option value="640">640</option>
                        <option value="672">672</option>
                        <option value="704">704</option>
                        <option value="736">736</option>
                        <option value="768">768</option>
                        <option value="800">800</option>
                        <option value="832">832</option>
                        <option value="864">864</option>
                        <option value="896">896</option>
                        <option value="928">928</option>
                        <option value="960">960</option>
                        <option value="992">992</option>
                        <option value="1024">1024</option>
                        <option value="1056">1056</option>
                    </select>
                </div>
                <div class="six columns">
                    <label for="serial_timeout">Таймаут чтения UART [мс]</label>
                    <input type="number" id="serial_timeout" name="serial_timeout" min="1" max="65535" value="50">
                </div>
            </div>
            <div id="rep_rssi_dbm_div" class="row">
                <div class="twelve columns">
                    <div class="checkbox-wrapper-14">
                        <input id="rep_rssi_dbm" name="rep_rssi_dbm" type="checkbox" class="switch">
                        <label for="rep_rssi_dbm">Показывать уровень сигнала в [dBm] вместо 0–100</label>
                    </div>
                </div>
            </div>
        </div>
    </form>
    <div class="row">
        <div class="six columns">
            <button class="button-primary" onclick="save_settings()">Сохранить настройки и перезагрузить</button>
        </div>
    </div>
</div>
<div class="container">
    <div id="esp_mac" style="margin-top: 1rem" class="row small_text"></div>
    <div id="about" class="row">Bridge для ESP32 — ожидание ответа от устройства
    </div>
    <div style="margin-bottom: 2rem" class="row">&copy; Wolfgang Christl 2018 - Apache 2.0 License</div>
</div>
<script>
    // get settings once upon page load
    while (get_system_info() < 0) {
    }
    while (get_settings() < 0) {
    }
    // get stats every second (less aggressive to reduce flicker)
    setInterval(get_stats, 1000)
    setInterval(update_conn_status, 1000)
    setInterval(check_for_issues, 1000)
    setInterval(function(){fetch("/api/link").then(function(r){return r.json();}).then(function(j){var el=document.getElementById("link_metrics");if(el)el.innerHTML="Пакеты: приём "+j.packets_received+" отпр "+j.packets_sent+" обработано "+j.packets_processed+" | Задержка: "+(j.connected?j.latency_ms+" мс":"-")+" | Потери: "+j.packet_drops+" ("+j.packet_loss_pct+"%)";}).catch(function(){});}, 2000)
    setTimeout(change_msp_ltm_visibility, 500)
    setTimeout(change_ap_ip_visibility, 500)
    setTimeout(change_uart_visibility, 500)
</script>
</body>
</html>

)BRIDGE_UI_RAW";

#endif /* BRIDGE_UI_EMBED_H */
