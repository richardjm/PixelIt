<template>
    <table>
        <tbody>
            <tr v-for="item in items" :key="item.title">
                <td class="name">{{ item.name }}:</td>
                <td>
                    {{ item.value }}
                </td>
            </tr>
        </tbody>
    </table>
</template>

<script setup>
    const props = defineProps({
         items: {
            type: Array,
            required: true,
        },
    })
</script>

<style>
.name {
    font-weight: 600;
    font-size: 16px !important;
}
</style>
