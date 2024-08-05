const outerRad = 48;
const innerRad = 44;
const outerArc = document.getElementById("outerArc");
const innerArc = document.getElementById("innerArc");
const nowTemp = document.getElementById("nowTemp");
const setTemp = document.getElementById("setTemp");
const burner = document.getElementById("burner");
const humidity = document.getElementById("humidity");
function getArc(radius, percentage) {
  const endAngle = 210 - (percentage / 100) * 240;
  const endCoords = getCoords(radius, endAngle);
  const startCoords = getCoords(radius, 210);
  const isLarge = endAngle < 30 ? 1 : 0;
  return `M ${startCoords}  A ${radius} ${radius} 0 ${isLarge} 1 ${endCoords}`;
}
function getCoords(rad, deg) {
  const x = rad * Math.cos((deg * Math.PI) / 180) + 50;
  const y = -rad * Math.sin((deg * Math.PI) / 180) + 50;
  return `${Math.round(x*100)/100},${Math.round(y*100)/100}`;
}

function updateNow(temp) {
  outerArc.setAttribute("d", getArc(48, (Math.max(0, temp - 15) / 15) * 100));
  nowTemp.textContent = temp;
}
function updateSet(temp) {
  innerArc.setAttribute("d", getArc(44, (Math.max(0, temp - 15) / 15) * 100));
  setTemp.textContent = temp;
}

function fetchData() {
  fetch("/data")
    .then((response) => response.json())
    .then((data) => {
      updateNow(data.temperature);
      updateSet(data.targetTemp);
			burner.setAttribute("fill", data.relay == 1 ? "red" : "#ccc");
			humidity.textContent = data.humidity;
    });
}
setInterval(fetchData, 2500);
fetchData();
