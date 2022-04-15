<template>
    <input type="button" :color="color" elevation="4" @click="onclick" :disabled="!condition" v-text="text" />
        <!-- <v-icon left>{{ icon }}</v-icon> -->
</template>

<script setup>
    const props = defineProps({
        condition: {
            type: Boolean,
            required: true,
        },
        text: {
            type: String,
            required: true,
        },
        color: {
            type: String,
            required: true,
        },
        onclick: {
            type: Function,
            required: false,
        },
        icon: {
            type: String,
            required: true,
        },
    })
</script>
