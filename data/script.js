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
  const custom = document
    .getElementById("fixedConfig")
    .getElementsByClassName("tempSelector")[0];
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
  if (mode.id == "scheduleActions") continue;
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

function newTempSelector(temp) {
  const newTempSelector = document.createElement("div");
  newTempSelector.className = "tempSelector";
  newTempSelector.innerHTML = `
    <button class="sub button">−</button>
    <div class="container">
      <input type="number" min="15" max="30" ${
        temp ? `value="${temp}"` : ""
      } class="tempInput"/>
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
						<input class="start time" type="time" name="start" ${
              start ? `value="${start}"` : ""
            }>
						<input class="end time" type="time" name="end" ${end ? `value="${end}"` : ""}>
            `;
  line.appendChild(newTempSelector(temp));
  return line;
}
let listClipboard;
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
        <div class="scheduleOptions">
          <button class="more button">
            <svg height="20" viewBox=".5 0 3 4" xmlns="http://www.w3.org/2000/svg">
              <path stroke-linecap="round" stroke="#777" stroke-width="0.4" fill="none" d="M2,1L1,2L2,3" />
            </svg>
          </button>
          <button class="copy bump" hidden>copy</button>
          <button class="paste bump" hidden>paste</button>
          <button class="remLine bump" hidden>remove line</button>
          <button class="addLine bump" hidden>add line</button>
        </div>
      </div>
      <ol class="scheduleList">
      </ol>
    `;
    scheduleItem
      .getElementsByClassName("more")[0]
      .addEventListener("click", () => {
        const options = scheduleItem.getElementsByClassName("bump");
        let hide = !options[0].hidden;
        for (const option of options) {
          option.hidden = hide;
        }
        scheduleItem.getElementsByTagName("h3")[0].hidden = !hide;
        scheduleItem
          .getElementsByTagName("path")[0]
          .setAttribute("d", !hide ? "M1,1L2,2L1,3" : "M2,1L1,2L2,3");
      });
    schedules.appendChild(scheduleItem);
    const scheduleList = scheduleItem.getElementsByClassName("scheduleList")[0];
    for (let j = 0; j < 3; j++) {
      scheduleList.appendChild(createScheduleLine());
    }
    const addline = scheduleItem.getElementsByClassName("addLine")[0];
    addline.addEventListener("click", () => {
      scheduleList.appendChild(createScheduleLine());
    });
    const remLine = scheduleItem.getElementsByClassName("remLine")[0];
    remLine.addEventListener("click", () => {
      scheduleList.removeChild(scheduleList.lastElementChild);
    });
    const copy = scheduleItem.getElementsByClassName("copy")[0];
    copy.addEventListener("click", () => {
      listClipboard = parseScheduleList(scheduleList);
      console.log(listClipboard);
    });
    const paste = scheduleItem.getElementsByClassName("paste")[0];
    paste.addEventListener("click", () => {
      scheduleList.innerHTML = "";
      for (const line of listClipboard) {
        scheduleList.appendChild(
          createScheduleLine(
            timeToString(line.start_time),
            timeToString(line.end_time),
            line.temperature
          )
        );
      }
      for (let i = listClipboard.length; i < 3; i++) {
        scheduleList.appendChild(createScheduleLine());
      }
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

function invalidTime(first, last) {
  console.log(first, last);
  console.log(isNaN(first[0]), isNaN(last[0]));
  return (
    first[0] > last[0] ||
    (first[0] == last[0] && first[1] > last[1]) ||
    isNaN(first[0]) ||
    isNaN(last[0])
  );
}

function parseScheduleList(scheduleList) {
  const schedule = [];
  let prevEnd = [0, 0];
  for (const line of scheduleList.getElementsByClassName("scheduleLine")) {
    const start = line
      .getElementsByClassName("start")[0]
      .value.split(":")
      .map((x) => parseInt(x));
    const end = line
      .getElementsByClassName("end")[0]
      .value.split(":")
      .map((x) => parseInt(x));
    const temp = parseInt(line.getElementsByClassName("tempInput")[0].value);
    if (isNaN(start[0]) && isNaN(end[0]) && isNaN(temp)) {
      continue;
    } else if (
      invalidTime(prevEnd, start) ||
      invalidTime(start, end) ||
      temp < 15 ||
      temp > 30 ||
      isNaN(temp)
    ) {
      line.style.boxShadow = "0 0 0 2px orange";
      setTimeout(() => {
        line.style.boxShadow = "";
      }, 1500);
      throw new Error("Invalid time or temperature");
    }
    schedule.push({
      start_time: start,
      end_time: end,
      temperature: temp,
    });
    prevEnd = end;
  }
  return schedule;
}

document.getElementById("save").addEventListener("click", () => {
  const selectedMode = document
    .getElementById("scheduleModes")
    .getElementsByClassName("selected")[0].id;
  const scheduleLists = document.getElementsByClassName("scheduleList");
  const completeSchedule = {};
  try {
    if (selectedMode == "single") {
      completeSchedule.schedule = parseScheduleList(scheduleLists[1]);
    } else if (selectedMode == "weekly") {
      completeSchedule.weekends = parseScheduleList(scheduleLists[0]);
      completeSchedule.weekdays = parseScheduleList(scheduleLists[1]);
    } else if (selectedMode == "daily") {
      for (let i = 0; i < 7; i++) {
        completeSchedule[i + 1] = parseScheduleList(scheduleLists[i]);
      }
    }
  } catch (e) {
    console.error(e);
    return;
  }
  fetch("/schedule/" + selectedMode, {
    method: "POST",
    body: JSON.stringify(completeSchedule),
  });
  fetchSchedule();
});

document.getElementById("discard").addEventListener("click", fetchSchedule);

function fetchSchedule() {
  fetch("/schedule")
    .then((response) => response.json())
    .then((data) => {
      const scheduleLists = document.getElementsByClassName("scheduleList");
      for (const [day, newScheduleList] of Object.entries(data)) {
        console.log(day, newScheduleList);
        const index = parseInt(day) - 1;
        scheduleLists[index].innerHTML = "";
        for (const schedule of newScheduleList) {
          scheduleLists[index].appendChild(
            createScheduleLine(
              timeToString(schedule.start_time),
              timeToString(schedule.end_time),
              schedule.temperature
            )
          );
        }
        for (let i = newScheduleList.length; i < 3; i++) {
          scheduleLists[index].appendChild(createScheduleLine());
        }
      }
    });
}

fetchSchedule();

function timeToString(time) {
  return `${time[0].toString().padStart(2, "0")}:${time[1]
    .toString()
    .padStart(2, "0")}`;
}
