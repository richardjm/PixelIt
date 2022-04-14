<template>
  <div>
    <NavLinks :items="getNavLinks()" />
    <router-view/>
  </div>
</template>

<script setup>
  import NavLinks from '@/components/NavLinks'
  import { useStore } from 'vuex'

  const store = useStore()

  function getNavLinks() {
    return store.state.navLinks;
  }
</script>

<style lang="scss">
#app {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-align: center;
  color: #2c3e50;
}

nav {
  padding: 30px;

  a {
    font-weight: bold;
    color: #2c3e50;

    &.router-link-exact-active {
      color: #42b983;
    }
  }
}
</style>
