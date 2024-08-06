const outerRad = 48;
const innerRad = 44;
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
  const outerArc = document.getElementById("outerArc");
  const nowTemp = document.getElementById("nowTemp");
  outerArc.setAttribute("d", getArc(outerRad, (Math.max(0, temp - 15) / 15) * 100));
  nowTemp.textContent = temp;
}
function updateSet(temp) {
  const innerArc = document.getElementById("innerArc");
  const setTemp = document.getElementById("setTemp");
  const custom = document.getElementById("tempInput")
  innerArc.setAttribute("d", getArc(innerRad, (Math.max(0, temp - 15) / 15) * 100));
  setTemp.textContent = temp;
  custom.value = temp;
}

function updateForce(forceState) {
  const innerArc = document.getElementById("innerArc");
  const setTemp = document.getElementById("setTemp");
  if (forceState == 0) {
    innerArc.setAttribute("d", getArc(innerRad, 0));
    setTemp.textContent = "OFF";
  } else if (forceState == 1) {
    innerArc.setAttribute("d", getArc(innerRad, 100));
    setTemp.textContent = "ON";
  }

}

function fetchData() {
  const burner = document.getElementById("burner");
  const humidity = document.getElementById("humidity");
  fetch("/data")
    .then((response) => response.json())
    .then((data) => {
      updateNow(data.temperature);
      updateSet(data.targetTemp);
      updateForce(data.forceState);
			burner.setAttribute("fill", data.relay == 1 ? "red" : "#ccc");
			humidity.textContent = data.humidity;
    })
    .catch(() => console.error("Error reaching server."));
}
setInterval(fetchData, 2500);
fetchData();


for (const mode of document.getElementsByClassName("modes")) {
  for (const option of mode.children) {
    option.addEventListener("click", () => {
      for (const otherOption of mode.children) {
          otherOption.classList.remove("selected");
      }
      option.classList.add("selected");
      if (mode.id == "mainModes") {
        document.getElementById("fixedConfig").style.display = option.id == "fixed" ? "flex" : "none";
        document.getElementById("scheduleConfig").style.display = option.id == "schedule" ? "flex" : "none";
      } else if (mode.id == "fixedModes") {
        document.getElementById("tempSelector").style.display = option.id == "custom" ? "flex" : "none";
      }
    });
  }
}

document.getElementById("off").addEventListener("click", () => {
  fetch("/set?force=0",{method: 'POST'});
});
document.getElementById("on").addEventListener("click", () => {
  fetch("/set?force=1",{method: 'POST'});
});
document.getElementById("custom").addEventListener("click", () => {
  fetch("/set?force="+document.getElementById("tempInput").value,{method: 'POST'});
});
document.getElementById("schedule").addEventListener("click", () => {
  fetch("/set?force=-1",{method: 'POST'});
});
document.getElementById("fixed").addEventListener("click", () => {
  for (const item of document.getElementById("fixedModes").children) {
    if (item.classList.contains("selected")) {
      item.click();
    }
  }
});

document.getElementById("add").addEventListener("click", () => {
  const input = document.getElementById("tempInput");
  const temp = Math.min(30, Math.max(15, parseInt(input.value) + 1 ||0))
  input.value = temp;
  fetch("/set?force="+temp,{method: 'POST'});
});

document.getElementById("sub").addEventListener("click", () => {
  const input = document.getElementById("tempInput");
  const temp = Math.min(30, Math.max(15, parseInt(input.value) - 1 ||0))
  input.value = temp;
  fetch("/set?force="+temp,{method: 'POST'});
});

let timeoutId;
document.getElementById("tempInput").addEventListener('input', (event) => {
  const inputValue = event.target.value;
  let nValue = parseInt(inputValue);
  if (inputValue.length > 2) {
    nValue = parseInt(inputValue.slice(0, 2));
    event.target.value = nValue;
  } 
  clearTimeout(timeoutId);
  timeoutId = setTimeout(() => {
    nValue = Math.min(30, Math.max(15, nValue));
    event.target.value = nValue;
    fetch("/set?force="+nValue,{method: 'POST'});
  }, 1500)
});
