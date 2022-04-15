import { createRouter, createWebHashHistory } from 'vue-router'
import Home from '@/views/Home.vue'
import Options from '@/views/Options.vue'

const routes = [
  {
    path: '/',
    name: 'Home',
    component: Home
  },
  {
    path: '/options',
    name: 'Options',
    component: Options
  },
  // {
  //   path: '/sensorsbuttons',
  //   name: 'Sensors & Buttons',
  //   component: SensorsButtons
  // },
  // {
  //   path: '/testarea',
  //   name: 'TestArea',
  //   component: TestArea
  // },
  // {
  //   path: '/update',
  //   name: 'Update',
  //   component: Update
  // },
  // {
  //   path: '/gallery',
  //   name: 'Gallery',
  //   component: Gallery
  // },
  // {
  //   path: '/creator',
  //   name: 'Creator',
  //   component: Creator
  // },
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

export default router
