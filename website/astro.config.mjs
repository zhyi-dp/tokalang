// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// https://astro.build/config
export default defineConfig({
	site: 'https://tokalang.dev',
	integrations: [
		starlight({
			title: 'Toka Lang',
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
						{ label: 'What is Toka?', slug: 'introduction' },
						{ label: 'Installation', slug: 'installation' },
					],
				},
				{
					label: 'Core Concepts',
					translations: { zh: '核心概念' },
					items: [
						{ label: 'Attribute Token System', slug: 'concepts/attributes' },
						{ label: 'Pointer Morphology', slug: 'concepts/pointers' },
						{ label: 'Ownership & RAII', slug: 'concepts/ownership' },
						{ label: 'Soul vs Identity', slug: 'concepts/soul-identity' },
					],
				},
				{
					label: 'Type System',
					translations: { zh: '类型系统' },
					items: [
						{ label: 'Shapes', slug: 'types/shapes' },
						{ label: 'Unions & Enums', slug: 'types/unions' },
						{ label: 'Tuples & Arrays', slug: 'types/arrays' },
					],
				},
				{
					label: 'Advanced',
					translations: { zh: '进阶特性' },
					items: [
						{ label: 'Async Programming', slug: 'advanced/async' },
						{ label: 'Generics & Traits', slug: 'advanced/generics' },
						{ label: 'Concurrency', slug: 'advanced/concurrency' },
					],
				},
			],
			customCss: [
				'./src/styles/custom.css',
			],
		}),
	],
});
