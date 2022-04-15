<template>
    <div class="Log" log-field>
        <textarea filled outlined v-model="log" readonly class="log" log-text rows="20"></textarea>
    </div>
</template>

<script setup>
    const props = defineProps({
         log: {
            type: String,
            required: true,
        },
    })
</script>

<style>
[log-text] {
    line-height: 1.4rem !important;
    font-size: 14px;
    color: black;
    resize: none;
}
</style>
