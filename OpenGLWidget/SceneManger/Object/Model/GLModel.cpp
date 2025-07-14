#include "GLModel.h"

#include <QTextStream>
#include <QFile>
#include <QDebug>

CGLModel::CGLModel(QObject* parent)
	:QObject(parent)
{
	pMesh_ = new CGLMesh(this);
}

CGLModel::~CGLModel()
{
	
}

void CGLModel::loadModel(const QString& fileName)
{
	QFile objFile(fileName);
	if (!objFile.exists()) {
		qDebug() << "File nnot found";
		return;
	}

	objFile.open(QIODevice::ReadOnly);
	QTextStream input(&objFile);

	QVector<vec3f> pos;
	QVector<vec2f> uv;
	QVector<vec3f> norm;

	while (!input.atEnd())
	{
		QString str = input.readLine();
		// 这个list表示的是一行数据
		QStringList list = str.split(" ");

		if (list[0] == "#") {
			qDebug() << "This is comment:" << str;
			continue;
		}
		else if (list[0] == "mtllib") {
			qDebug() << "File with materials:" << list[1];
			continue;
		}
		else if (list[0] == "v") {  // 以 v 开头的行定义了一个几何顶点
			pos.append(vec3f{ list[1].toFloat(), list[2].toFloat(), list[3].toFloat() });
			continue;
		}
		else if (list[0] == "vt") { // 以 vt 开头的行定义了一个纹理坐标
			uv.append(vec2f{ list[1].toFloat(), list[2].toFloat() });
			continue;
		}
		else if (list[0] == "vn") { // 以 vn 开头的行定义了一个顶点法线
			norm.append(vec3f{ list[1].toFloat(), list[2].toFloat(), list[3].toFloat() });
			continue;
		}
		else if (list[0] == "f") {  // 以 f 开头的行定义一个多边形面，通过引用先前定义的顶点、纹理坐标和法线的索引来构建几何体
			for (int i = 1; i <= 3; ++i) {

				// list[i] 的格式为 "v/vt/vn"，其中 v 是顶点索引，vt 是纹理坐标索引，vn 是法线索引
				QStringList attrIdx = list[i].split("/");
				vertices_.append(
					VertexAttr{
						pos[attrIdx[0].toInt() - 1],
						uv[attrIdx[1].toInt() - 1],
						norm[attrIdx[2].toInt() - 1],
						vec3f{0.0, 0.0, 0.0},
						vec3f{0.0, 0.0, 0.0}
					}
				);
				indices_.append(static_cast<GLuint>(indices_.size()));
			}
			continue;
		}
		else if (list[0] == "usemtl") {
			qDebug() << "This is used naterial:" << list[1];
		}
	}

	objFile.close();
}

void CGLModel::readvi(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices)
{

	QFile file("D:/LMEngine.txt");
	//打开文件，不存在则创建
	file.open(QIODevice::Truncate | QIODevice::WriteOnly| QIODevice::Text);
	QTextStream out(&file);
	//写入文件需要字符串为QByteArray格式
	for (int i = 0; i < vertices.size(); i++)
	{
		out << "pos: " << vertices[i].pos.x << " " << vertices[i].pos.y << " " << vertices[i].pos.z << "\n"
			<< "uv: " << vertices[i].uv.x << " " << vertices[i].uv.y << "\n"
			<< "norm: " << vertices[i].norm.x << " " << vertices[i].norm.y << " " << vertices[i].norm.z << "\n"
			<< "tan: " << vertices[i].tan.x << " " << vertices[i].tan.y << " " << vertices[i].tan.z << "\n"
			<< "bitan: " << vertices[i].bitan.x << " " << vertices[i].bitan.y << " " << vertices[i].bitan.z << "\n" << "\n";
	}
	out << "EBO: " << "\n";
	for (int i = 0; i < indices.size(); i++)
	{
		out << indices[i] << " ";
	}

	file.close();
}

void CGLModel::initialize(const QString& fileName)
{
	// 初始化
	initializeOpenGLFunctions();
	loadModel(fileName);
    calculateTBN();

	static bool once = true;
	if (once)
	{
		qDebug() << vertices_.size() << " " << indices_.size();
		//readvi(vertices_, indices_);
		once = false;
	}
	

    pMesh_->initialize(vertices_, indices_);
	// pMesh_将会在内部创建VAO,VBO,EBO等，创建成功后就不需要vertices_和indices_了
    vertices_.clear();
    indices_.clear();
}

void CGLModel::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos, const glm::vec3& viewPos)
{
	if (!pMesh_) {
		qDebug() << "Mesh is not initialized.";
		return;
	}

	pMesh_->draw(view, projection, lightPos, viewPos);
}

void CGLModel::move(const QPoint& pos, const float& scale)
{
	// 更改模型的M矩阵即可
	pMesh_->move(pos, scale);
}

void CGLModel::calculateTBN()
{
	// 以三个顶点为一组，计算这三个顶点的TBN矩阵（计算出来的TBN矩阵基于物体空间坐标系，在着色器中需左乘法线矩阵转换到世界空间坐标系）
	for (int i = 0; i < vertices_.size(); i += 3)
	{
		// 计算切线和副切线，计算结果不是TBN矩阵中的T和B
		float deltaU1 = vertices_[i + 1].uv.x - vertices_[i].uv.x;
		float deltaU2 = vertices_[i + 2].uv.x - vertices_[i].uv.x;

		float deltaV1 = vertices_[i + 1].uv.y - vertices_[i].uv.y;
		float deltaV2 = vertices_[i + 2].uv.y - vertices_[i].uv.y;

		glm::vec3 E1 = glm::vec3{
			vertices_[i + 1].pos.x - vertices_[i].pos.x,
			vertices_[i + 1].pos.y - vertices_[i].pos.y,
			vertices_[i + 1].pos.z - vertices_[i].pos.z
		};

		glm::vec3 E2 = glm::vec3{
			vertices_[i + 2].pos.x - vertices_[i].pos.x,
			vertices_[i + 2].pos.y - vertices_[i].pos.y,
			vertices_[i + 2].pos.z - vertices_[i].pos.z
		};

		float mu = 1 / (deltaU1 * deltaV2 - deltaV1 * deltaU2);

		glm::vec3 tagent = mu * glm::vec3{ deltaV2 * E1 - deltaV1 * E2 };

		glm::vec3 bitangent = mu * glm::vec3{ -deltaU2 * E1 - deltaU1 * E2 };

		// 此时我们得到了tagent和bitangent，但他们：1. 没有和法线正交化；2. 没有归一化

		// 施密特正交化
		glm::vec3 normal = {
			vertices_[i].norm.x,
			vertices_[i].norm.y,
			vertices_[i].norm.z
		};
		//											// 法线的方向				// 点乘结果表示tagent在法线方向上的投影长度
		glm::vec3 T = glm::normalize(tagent - glm::normalize(normal) * glm::dot(tagent, glm::normalize(normal)));
		glm::vec3 B = glm::normalize(glm::cross(normal, T));
		glm::vec3 N = glm::normalize(normal);

		// 注意结果基于物体空间坐标系
		for (int j = 0; j < 3; j++)
		{
			vertices_[i + j].tan.x = T.x;
			vertices_[i + j].tan.y = T.y;
			vertices_[i + j].tan.z = T.z;

			vertices_[i + j].bitan.x = B.x;
			vertices_[i + j].bitan.y = B.y;
			vertices_[i + j].bitan.z = B.z;

			vertices_[i + j].norm.x = N.x;
			vertices_[i + j].norm.y = N.y;
			vertices_[i + j].norm.z = N.z;
		}
	}
}