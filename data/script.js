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
  return `${Math.round(x * 100) / 100},${Math.round(y * 100) / 100}`;
}

function updateNow(temp) {
  const outerArc = document.getElementById("outerArc");
  const nowTemp = document.getElementById("nowTemp");
  outerArc.setAttribute(
    "d",
    getArc(outerRad, (Math.max(0, temp - 15) / 15) * 100)
  );
  nowTemp.textContent = temp;
}
function updateSet(temp) {
  const innerArc = document.getElementById("innerArc");
  const setTemp = document.getElementById("setTemp");
  const custom = document.getElementById("tempInput");
  innerArc.setAttribute(
    "d",
    getArc(innerRad, (Math.max(0, temp - 15) / 15) * 100)
  );
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

const fixedTempSelector = newTempSelector();
const fixedTempInput = fixedTempSelector.getElementsByClassName("tempInput")[0];
document.getElementById("fixedConfig").appendChild(fixedTempSelector);
fixedTempSelector
  .getElementsByClassName("sub")[0]
  .addEventListener("click", () => {
    fetch("/set?force=" + fixedTempInput.value, { method: "POST" });
  });
fixedTempSelector
  .getElementsByClassName("add")[0]
  .addEventListener("click", () => {
    fetch("/set?force=" + fixedTempInput.value, { method: "POST" });
  });
let timeoutId;
fixedTempInput.addEventListener("input", (event) => {
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
    fetch("/set?force=" + nValue, { method: "POST" });
  }, 1500);
});

for (const mode of document.getElementsByClassName("modes")) {
  for (const option of mode.children) {
    option.addEventListener("click", () => {
      for (const otherOption of mode.children) {
        otherOption.classList.remove("selected");
      }
      option.classList.add("selected");
      if (mode.id == "mainModes") {
        document.getElementById("fixedConfig").hidden = option.id != "fixed";
        document.getElementById("scheduleConfig").hidden =
          option.id != "schedule";
      } else if (mode.id == "fixedModes") {
        document
          .getElementById("fixedConfig")
          .getElementsByClassName("tempSelector")[0].hidden =
          option.id != "custom";
      }
    });
  }
}

document.getElementById("off").addEventListener("click", () => {
  fetch("/set?force=0", { method: "POST" });
});
document.getElementById("on").addEventListener("click", () => {
  fetch("/set?force=1", { method: "POST" });
});
document.getElementById("custom").addEventListener("click", () => {
  fetch("/set?force=" + fixedTempInput.value, {
    method: "POST",
  });
});
document.getElementById("schedule").addEventListener("click", () => {
  fetch("/set?force=-1", { method: "POST" });
});
document.getElementById("fixed").addEventListener("click", () => {
  for (const item of document.getElementById("fixedModes").children) {
    if (item.classList.contains("selected")) {
      item.click();
    }
  }
});

function newTempSelector() {
  const newTempSelector = document.createElement("div");
  newTempSelector.className = "tempSelector";
  newTempSelector.innerHTML = `
    <button class="sub button">−</button>
    <div class="container">
      <input type="number" min="15" max="30" class="tempInput"/>
    </div>
    <button class="add button">+</button>
  `;
  const newTempInput = newTempSelector.getElementsByClassName("tempInput")[0];
  newTempSelector
    .getElementsByClassName("sub")[0]
    .addEventListener("click", () => {
      newTempInput.value = Math.min(
        30,
        Math.max(15, parseInt(newTempInput.value) - 1 || 0)
      );
    });
  newTempSelector
    .getElementsByClassName("add")[0]
    .addEventListener("click", () => {
      newTempInput.value = Math.min(
        30,
        Math.max(15, parseInt(newTempInput.value) + 1 || 0)
      );
    });
  return newTempSelector;
}

function createScheduleLine(start, end, temp) {
  const line = document.createElement("div");
  line.className = "scheduleLine";
  line.innerHTML = `
						<input class="time" type="time" name="start">
						<input class="time" type="time" name="end">
            `;
  line.appendChild(newTempSelector());
  return line;
}

function createSchedules() {
  const schedules = document.getElementById("schedules");
  const days = [
    "sunday",
    "monday",
    "tuesday",
    "wednesday",
    "thursday",
    "friday",
    "saturday",
  ];
  for (let i = 0; i < 7; i++) {
    const scheduleItem = document.createElement("li");
    scheduleItem.className = "schedule";
    scheduleItem.innerHTML = `
      <div class="scheduleHeader">
        <h3>${days[i]}</h3>
        <button class="addline">+ add line</button>
      </div>
      <ol class="scheduleList">
      </ol>
    `;
    schedules.appendChild(scheduleItem);
    const scheduleList = scheduleItem.getElementsByClassName("scheduleList")[0];
    for (let j = 0; j < 3; j++) {
      scheduleList.appendChild(createScheduleLine());
    }
    const addline = scheduleItem.getElementsByClassName("addline")[0];
    addline.addEventListener("click", () => {
      scheduleList.appendChild(createScheduleLine());
    });
  }
}
createSchedules();

document.getElementById("single").addEventListener("click", () => {
  for (const scheduleDay of document.getElementsByClassName("schedule")) {
    scheduleDay.hidden = true;
  }
  document
    .getElementsByClassName("schedule")[1]
    .getElementsByTagName("h3")[0].innerText = "everyday";
  document.getElementsByClassName("schedule")[1].hidden = false;
});

document.getElementById("weekly").addEventListener("click", () => {
  for (const scheduleDay of document.getElementsByClassName("schedule")) {
    scheduleDay.hidden = true;
  }
  document
    .getElementsByClassName("schedule")[0]
    .getElementsByTagName("h3")[0].innerText = "weekends";
  document
    .getElementsByClassName("schedule")[1]
    .getElementsByTagName("h3")[0].innerText = "weekdays";
  document.getElementsByClassName("schedule")[0].hidden = false;
  document.getElementsByClassName("schedule")[1].hidden = false;
});

document.getElementById("daily").addEventListener("click", () => {
  for (const scheduleDay of document.getElementsByClassName("schedule")) {
    scheduleDay.hidden = false;
  }
  document
    .getElementsByClassName("schedule")[0]
    .getElementsByTagName("h3")[0].innerText = "sunday";
  document
    .getElementsByClassName("schedule")[1]
    .getElementsByTagName("h3")[0].innerText = "monday";
});
