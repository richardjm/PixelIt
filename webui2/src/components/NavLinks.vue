<template>
    <nav>
        <img alt="Vue logo" src="../assets/logo.png" />
        
        <div v-for="item in items" :key="item.title">
            <router-link :to="item.page" v-if="item.page">{{ item.title }}</router-link>
            <a :href="item.url" v-if="item.url">{{ item.title }}</a>
        </div>
    </nav>
</template>

<script setup>
    const props = defineProps({
         items: {
            type: Array,
            required: true,
        },
    })
</script>
