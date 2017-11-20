import React from 'react';						// eslint-disable-line no-unused-vars
import ReactDOM from 'react-dom';
import { HashRouter, Link, Route, Switch } from 'react-router-dom';	// eslint-disable-line no-unused-vars

import { Navbar, Nav, NavItem } from 'react-bootstrap';			// eslint-disable-line no-unused-vars
import { PageHeader } from 'react-bootstrap';				// eslint-disable-line no-unused-vars
import { Grid, Row, Col } from 'react-bootstrap';			// eslint-disable-line no-unused-vars
import Lighting from './lighting.js';
import Heating from './heating.js';
import { LinkContainer } from 'react-router-bootstrap';			// eslint-disable-line no-unused-vars

const App = () => ( // eslint-disable-line no-unused-vars
	<div>
		<Navbar inverse>
			<Navbar.Header>
				<Navbar.Brand>
					<a href="#">Гавань</a>
				</Navbar.Brand>
				<Navbar.Toggle />
			</Navbar.Header>
			<Navbar.Collapse>
				<Nav>
					<LinkContainer to="lighting">
						<NavItem>Освещение</NavItem>
					</LinkContainer>
					<LinkContainer to="heating">
						<NavItem>Климат</NavItem>
					</LinkContainer>
					<LinkContainer to="about">
						<NavItem>О системе</NavItem>
					</LinkContainer>
				</Nav>
			</Navbar.Collapse>
		</Navbar>
		<Grid>
			<Row>
				<Col xs={12} md={12}>
					<Switch>
						<Route path='/lighting' component={Lighting}/>
						<Route path='/heating' component={Heating}/>
						<Route path='/about' component={About}/>
					</Switch>
				</Col>
			</Row>
		</Grid>
	</div>
);

const About = () => (
	<div>
		<PageHeader>О системе</PageHeader>
		<p>Управление всем, что поддается управлению в доме.</p>
		<p>Версия: 0.2.</p>
		<p>Дата: 14-11-2017.</p>
		<p>Оды, хвалебные стихи, благодарственные псалмы и если что-то не так: <a href="mailto:denis.afanassiev@gmail.com">denis.afanassiev@gmail.com</a></p>
	</div>
);

ReactDOM.render((
	<HashRouter>
		<App />
	</HashRouter>
), document.getElementById('app'));
