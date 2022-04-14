import { createStore } from 'vuex'

export default createStore({
  state: {
    navLinks: [
      { title: "Dashboard", icon: "mdi-memory", page: "/" },
      { title: "Options", icon: "mdi-tune-vertical", page: "/options" },
      { title: "Sensors & Buttons", icon: "mdi-gesture-tap-button", page: "/sensorsbuttons" },
      { title: "Test Area", icon: "mdi-cube-outline", page: "/testarea" },
      { title: "Update", icon: "mdi-tray-arrow-up", page: "/update" },
      //{ title: "Pixel Gallery", icon: "mdi-image-outline", url: "https://pixelit.bastelbunker.de/PixelGallery", target: "_blank" },
      { title: "Pixel Gallery", icon: "mdi-image-outline", page: "/gallery" },
      { title: "Pixel Creator", icon: "mdi-pencil-box-outline", url: "https://pixelit.bastelbunker.de/PixelCreator", target: "_blank" },
      //{ title: "Pixel Creator", icon: "mdi-pencil-box-outline", page: "/creator" },
      { title: "Forum", icon: "mdi-forum-outline", url: "https://github.com/o0shojo0o/PixelIt/discussions", target: "_blank" },
      { title: "Blog", icon: "mdi-post-outline", url: "https://www.bastelbunker.de/pixel-it/", target: "_blank" },
      { title: "Documentation", icon: "mdi-book-open-page-variant-outline", url: "https://docs.bastelbunker.de/pixelit/", target: "_blank" },
      { title: "GitHub", icon: "mdi-github", url: "https://github.com/o0shojo0o/PixelIt", target: "_blank" },
    ],
  },
  getters: {
  },
  mutations: {
  },
  actions: {
  },
  modules: {
  }
})
