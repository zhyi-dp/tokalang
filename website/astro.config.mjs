// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import fs from 'fs';

const tokaLanguage = JSON.parse(fs.readFileSync(new URL('./src/toka.tmLanguage.json', import.meta.url), 'utf-8'));

// https://astro.build/config
export default defineConfig({
	site: 'https://tokalang.dev',
	integrations: [
		starlight({
			title: 'Toka Lang',
			expressiveCode: {
				shiki: {langs: [tokaLanguage]}
			},
			logo: {
				src: './src/assets/logo.svg',
			},
			social: [
				{ icon: 'github', label: 'GitHub', href: 'https://github.com/tokalang/toka' },
			],
			locales: {
				root: {
					label: 'English',
					lang: 'en',
				},
				zh: {
					label: '简体中文',
					lang: 'zh-CN',
				},
			},
			sidebar: [
				{
					label: 'Introduction',
					translations: { 'zh-CN': '简介' },
					items: [
						{ label: 'Try Toka Online', translations: { 'zh-CN': '在线运行 (Playground)' }, link: '/playground/' },
						{ label: 'What is Toka?', translations: { 'zh-CN': 'Toka 是什么？' }, slug: 'introduction' },
						{ label: 'Installation', translations: { 'zh-CN': '安装' }, slug: 'installation' },
						{ label: 'CLI & Tooling', translations: { 'zh-CN': '命令行工具与构建' }, slug: 'cli-tooling' },
						{ label: 'Project Structure', translations: { 'zh-CN': '项目结构与最佳实践' }, slug: 'project-structure' },
					],
				},
				{
					label: 'Learn Toka (Toka Book)',
					translations: { 'zh-CN': '深入学习 (Toka 教程书)' },
					link: 'https://lumicore-dev.github.io/toka-book/',
				},
			],
			customCss: [
				'./src/styles/custom.css',
			],
		}),
	],
});
