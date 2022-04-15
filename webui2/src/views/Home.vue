<template>
  <div class="home">
    <div class="system">
      <h1>System</h1>
      <hr />
      <ListInfo :items="systemItems" />
    </div>
    <div class="sensors">
      <h1>Sensors</h1>
      <hr />
      <ListInfo :items="sensorItems" />
    </div>
    <div class="buttons">
      <h1>Buttons</h1>
      <hr />
      <ListInfo :items="buttonItems" />
    </div>
    <div class="logs">
      <h1>Logs</h1>
      <hr />
      <Log :log="log" />
    </div>
  </div>
</template>

<script setup>
  import ListInfo from '@/components/ListInfo'
  import { useStore } from 'vuex'

  const store = useStore()

  function systemItems() {
    return store.state.sysInfoData;
  }
  function sensorItems() {
    return store.state.sensorData;
  };
  function buttonItems() {
    return store.state.buttonData;
  };
  function log() {
    return store.state.logData.join("\n");
  };
</script>
