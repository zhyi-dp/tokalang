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
				src: './src/assets/logo.png',
			},
			social: [
				{ icon: 'github', label: 'GitHub', href: 'https://github.com/zhyi-dp/tokalang' },
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
					translations: { zh: '简介' },
					items: [
						{ label: 'What is Toka?', translations: { zh: 'Toka 是什么？' }, slug: 'introduction' },
						{ label: 'Installation', translations: { zh: '安装' }, slug: 'installation' },
					],
				},
				{
					label: 'Core Concepts',
					translations: { zh: '核心概念' },
					items: [
						{ label: 'Attribute Token System', translations: { zh: '属性标记系统' }, slug: 'concepts/attributes' },
						{ label: 'Pointer Morphology', translations: { zh: '指针形态与帽氏法则' }, slug: 'concepts/pointers' },
						{ label: 'Ownership & RAII', translations: { zh: '所有权与 RAII' }, slug: 'concepts/ownership' },
						{ label: 'Soul vs Identity', translations: { zh: '灵与壳 (Soul vs Identity)' }, slug: 'concepts/soul-identity' },
					],
				},
				{
					label: 'Type System',
					translations: { zh: '类型系统' },
					items: [
						{ label: 'Shapes', translations: { zh: '统一结构 (Shapes)' }, slug: 'types/shapes' },
						{ label: 'Unions & Enums', translations: { zh: '联合与枚举' }, slug: 'types/unions' },
						{ label: 'Tuples & Arrays', translations: { zh: '元组与数组' }, slug: 'types/arrays' },
						{ label: 'Interfaces & Traits', translations: { zh: '接口与特征 (Traits)' }, slug: 'types/traits' },
					],
				},
				{
					label: 'Advanced',
					translations: { zh: '进阶特性' },
					items: [
						{ label: 'Async Programming', translations: { zh: '异步与协程' }, slug: 'advanced/async' },
						{ label: 'Generics', translations: { zh: '泛型 (Generics)' }, slug: 'advanced/generics' },
						{ label: 'Concurrency & Locks', translations: { zh: '并发与 RAII 锁' }, slug: 'advanced/concurrency' },
					],
				},
			],
			customCss: [
				'./src/styles/custom.css',
			],
		}),
	],
});
