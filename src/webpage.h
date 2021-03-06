const char INDEX_HTML[] PROGMEM = R"=====(
  <html>

  <head>
    <!-- See https://css-tricks.com/aspect-ratio-boxes/ -->
    <style>
      .steering {
        margin: auto;
        background-color: blue;
        overflow: hidden;
        height: 0;
        padding-top: 85%;
        width: 85%;
      }

      #status {
        margin: auto;
      }

      body {
        font-size: 12pt;
      }
    </style>

  </head>

  <body>
    <h1>Brad's Fantastic Robot Steerer</h1>
    <input type="textarea" id="status" disabled />
    <div class="steering" id="steering"></div>
    <div id="ledstatus"><b>LED</b></div>
    <button id="ledon" type="button" onclick="buttonclick(this);">On</button>
    <button id="ledoff" type="button" onclick="buttonclick(this);">Off</button>

    <script>
      with(document.getElementById('steering')) {
        addEventListener('mousedown', startMove);
        addEventListener('touchstart', startMove);
        addEventListener('mouseup', endMove);
        addEventListener('touchend', endMove);
      }

      var websock;
      websock = new WebSocket('ws://' + window.location.hostname + ':81/');
      websock.onopen = function(evt) {
        console.log('websock open');
      };
      websock.onclose = function(evt) {
        console.log('websock close');
      };
      websock.onerror = function(evt) {
        console.log(evt);
      };
      websock.onmessage = function(evt) {
        console.log(evt);
        var e = document.getElementById('ledstatus');
        if (evt.data === 'ledon') {
          e.style.color = 'red';
        } else if (evt.data === 'ledoff') {
          e.style.color = 'black';
        } else {
          console.log('unknown event');
        }
      };

      function buttonclick(e) {
        websock.send(e.id);
      }

      function startMove(e) {
        document.getElementById('steering').addEventListener('mousemove', move);
        document.getElementById('steering').addEventListener('touchmove', move);
      }

      function endMove(e) {
        document.getElementById('steering').removeEventListener('mousemove', move);
        document.getElementById('steering').removeEventListener('touchmove', move);
      }

      function getPosition(e) {

        var x = (e.type == "touchmove") ? e.touches[0].clientX : e.clientX;
        var y = (e.type == "touchmove") ? e.touches[0].clientY : e.clientY;

        var rect = e.target.getBoundingClientRect();
        x = Math.floor(((x - rect.left) / rect.width - 0.5) * 200 + 0.5);
        y = Math.floor(((y - rect.top) / rect.height - 0.5) * 200 + 0.5);

        return {
          x,
          y
        }
      }

      function move(e) {
        var position = getPosition(e);
        document.getElementById('status').value = 'X: ' + position.x + ' Y: ' + position.y;
        websock.send("" + position.x + ',' + position.y);
      }
    </script>
  </body>

  </html>
)=====";
