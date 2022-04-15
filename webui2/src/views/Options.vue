<template>
  <div class="options">
    <div class="buttons">
      <ButtonCondition color="success" :condition="isValid && sockedIsConnected" text="Save config" @click="save" icon="mdi-content-save" />
    </div>
    <div>
      <h2>Defaults</h2>
      <hr />
      <input v-model="config.matrixBrightness" type="number" label="Matrix start brightness" hint="0 to 255" :rules="[rules.required, rules.min0, rules.max255]" />
      <input v-model="config.scrollTextDefaultDelay" type="number" label="Scrolltext delay" hint="larger number is slower" suffix="milliseconds" :rules="[rules.required, rules.min0]" />
      <input v-model="config.hostname" label="Hostname" />
      <input v-model="config.note" label="Note" />
      <input type="checkbox" v-model="config.bootScreenAktiv" label="Bootsceen active" hide-details dense />
    </div>
    <div>
      <h2>Matrix</h2>
      <hr />
      <select :items="matrixTypes" v-model="config.matrixTypes" label="Matrix type"></select>
      <select :items="matrixCorrection" v-model="config.matrixTempCorrection" label="Matrix correction"></select>
    </div>
    <div>
      <h2>Auto brightness</h2>
      <hr />
      <input type="checkbox" v-model="config.matrixBrightnessAutomatic" label="Auto brightness active" dense />
      <input v-model="config.mbaDimMin" label="Min bright" hint="0 to 255" type="number" :disabled="!config.matrixBrightnessAutomatic" :rules="config.matrixBrightnessAutomatic ? [rules.required, rules.min0, rules.max255] : []" dense />
      <input v-model="config.mbaDimMax" label="Max bright" hint="0 to 255" type="number" :disabled="!config.matrixBrightnessAutomatic" :rules="config.matrixBrightnessAutomatic ? [rules.required, rules.min0, rules.max255] : []" dense />
      <input v-model="config.mbaLuxMin" label="From lux" type="number" :disabled="!config.matrixBrightnessAutomatic" :rules="config.matrixBrightnessAutomatic ? [rules.required, rules.min0] : []" dense />
      <input v-model="config.mbaLuxMax" label="To lux" type="number" :disabled="!config.matrixBrightnessAutomatic" :rules="config.matrixBrightnessAutomatic ? [rules.required, rules.min0] : []" dense />
    </div>
      <div>
      <h2>Clock</h2>
      <hr />
      <input v-model="config.ntpServer" label="NTP-Server" hint="domain or ip address" :rules="[rules.required]" />
      <input v-model="config.clockTimeZone" type="number" label="UTC offset" hint="your UTC time offset" :rules="[rules.required, rules.minMinus12, rules.max14]" />
      <ColorPickerTextfield />
      <input type="checkbox" v-model="config.clockDayLightSaving" label="Daylight saving" dense hide-details />
      <input type="checkbox" v-model="config.clock24Hours" label="24 Hours" persistent-hint dense hide-details />
      <input type="checkbox" v-model="config.clockWithSeconds" label="Clock with sek" :disabled="!config.clock24Hours" dense hide-details />
      <input type="checkbox" v-model="config.clockSwitchAktiv" label="Switch clock/date active" dense hide-details />
      <input type="checkbox" v-model="config.clockDayOfWeekFirstMonday" label="Monday as start of the week" dense hide-details />
      <input type="checkbox" v-model="config.clockDateDayMonth" label="Date format DD.MM." dense />

      <input v-model="config.clockSwitchSec" type="number" label="Switch clock/date time" hint="the unit is seconds (s)" suffix="seconds" :disabled="!config.clockSwitchAktiv" :rules="config.clockSwitchAktiv ? [rules.required, rules.min0] : []" />
      <input type="checkbox" v-model="config.clockAutoFallbackActive" label="Clock auto fallback" dense />
      <select :items="autoFallbackAnimation" v-model="config.clockAutoFallbackAnimation" label="Fallback Animation" :disabled="!config.clockAutoFallbackActive"></select>
      <input v-model="config.clockAutoFallbackTime" type="number" label="Fallback time" hint="the unit is seconds (s)" suffix="seconds" :disabled="!config.clockAutoFallbackActive" :rules="config.clockSwitchAktiv ? [rules.required, rules.min0] : []" />
    </div>
    <div>
      <h2>MQTT</h2>
      <hr />
      <input type="checkbox" v-model="config.mqttAktiv" label="MQTT active" dense />
      <input v-model="config.mqttServer" label="Server" hint="domain or ip address" :disabled="!config.mqttAktiv" :rules="config.mqttAktiv ? [rules.required] : []" />
      <input v-model="config.mqttPort" label="Port" type="number" :disabled="!config.mqttAktiv" :rules="config.mqttAktiv ? [rules.required, rules.portRange] : []" />
      <input v-model="config.mqttUser" label="User" hint="optional" :disabled="!config.mqttAktiv" />
      <input v-model="config.mqttPassword" label="Passsword" hint="optional" :disabled="!config.mqttAktiv" />
      <input v-model="config.mqttMasterTopic" label="Master topic" :disabled="!config.mqttAktiv" :rules="config.mqttAktiv ? [rules.required] : []" />
    </div>
    <div>
      <h2>Sound</h2>
      <hr />
      <input v-model="config.initialVolume" type="number" label="Start volume" hint="Between 1 and 30" :rules="[rules.required, rules.volumeRange]" />
      <select :items="pinsESP8266" v-model="config.dfpRXpin" label="DFPlayer RX pin (ESP8266 only)" :disabled="!config.isESP8266"></select>
      <select :items="pinsESP8266" v-model="config.dfpTXpin" label="DFPlayer TX pin (ESP8266 only)" :disabled="!config.isESP8266"></select>
    </div>
  </div>
</template>

<script setup>
  import ColorPickerTextfield from "@/components/ColorPickerTextfield";
  import ButtonCondition from "@/components/ButtonCondition";
  import { computed } from 'vue'
  import { useStore } from 'vuex'

  const store = useStore()

  const isValid = true;

  const rules = computed(() => {
    return store.state.rules;
  })
  const config = computed(() => {
    return store.state.configData;
  })
  const sockedIsConnected = computed(() => {
    return store.state.socket.isConnected;
  })
  const matrixTypes = computed(() => {
    return store.state.matrixTypes;
  })
  const matrixCorrection = computed(() => {
    return store.state.matrixCorrection;
  })
  const autoFallbackAnimation = computed(() => {
    return store.state.autoFallbackAnimation;
  })
  const temperatureUnits = computed(() => {
    return store.state.temperatureUnits;
  })
  const ldrDevices = computed(() => {
    return store.state.ldrDevices;
  })
  const pinsESP8266 = computed(() => {
    return store.state.pinsESP8266;
  })

  function save() {
    // this.$socket.sendObj({ setConfig: this.config });
    // setTimeout(() => {this.$socket.close();
    // }, 3000);
  }
  // watch: {
  //     "$store.state.configData.clock24Hours": function(newVal) {
  //         if (newVal == false) {
  //             store.state.configData.clockWithSeconds = newVal;
  //         }
  //     },
  // },
</script>


<style>
div.options > div > input, select {
  display: block;
}
</style>
